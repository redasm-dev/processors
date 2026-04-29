#include "x86_lifter.h"
#include "x86_common.h"
#include <Zydis/Zydis.h>

static void _x86_lift_displ_addr(RDILInstruction* il, const RDOperand* op) {
    bool has_base = op->displ.base != ZYDIS_REGISTER_NONE;
    bool has_index = op->displ.index != ZYDIS_REGISTER_NONE;
    bool has_scale = op->displ.scale > 1;
    bool has_disp = op->displ.displ != 0;
    bool has_indexscale = has_index && has_scale;
    bool has_lhs = has_base || has_index;

    if(!has_lhs && !has_disp) {
        rd_il_unkn(il);
        return;
    }

    if(has_lhs && has_disp) rd_il_add(il);

    if(has_lhs) {
        if(has_base && has_indexscale) rd_il_add(il);
        if(has_base) rd_il_reg(il, op->displ.base);

        if(has_indexscale) {
            if(has_scale) {
                rd_il_mul(il);
                rd_il_reg(il, op->displ.index);
                rd_il_uint(il, op->displ.scale);
            }
            else
                rd_il_reg(il, op->displ.index);
        }
    }

    if(has_disp) rd_il_sint(il, op->displ.displ);
}

static void _x86_lift_op(RDILInstruction* il, const RDInstruction* instr,
                         usize idx) {
    const RDOperand* op = &instr->operands[idx];

    switch(op->kind) {
        case RD_OP_REG: rd_il_reg(il, op->reg); return;
        case RD_OP_ADDR: rd_il_var(il, op->addr); return;
        case RD_OP_IMM: rd_il_uint(il, op->imm); return;

        case RD_OP_MEM: {
            if(instr->id == ZYDIS_MNEMONIC_LEA)
                rd_il_var(il, op->mem);
            else {
                rd_il_mem(il);
                rd_il_var(il, op->mem);
            }

            return;
        }

        case RD_OP_PHRASE: {
            rd_il_add(il);
            rd_il_reg(il, op->phrase.base);
            rd_il_reg(il, op->phrase.index);
            return;
        }

        case RD_OP_DISPL: {
            bool is_lea_src = (instr->id == ZYDIS_MNEMONIC_LEA) && (idx > 0);
            if(!is_lea_src) rd_il_mem(il);
            _x86_lift_displ_addr(il, op);
            return;
        }

        default: rd_il_unkn(il); return;
    }
}

static void _x86_lift_jump(RDILInstruction* il, const RDInstruction* instr) {
    rd_il_if(il);

    switch(instr->id) {
        case ZYDIS_MNEMONIC_JZ:
            rd_il_eq(il);
            rd_il_sym(il, "z");
            rd_il_uint(il, 0);
            break;

        case ZYDIS_MNEMONIC_JNZ:
            rd_il_ne(il);
            rd_il_sym(il, "z");
            rd_il_uint(il, 0);
            break;

        default:
            rd_il_unkn(il); // cond
            break;
    }

    // t: taken branch target (operand 0)
    _x86_lift_op(il, instr, 0);

    // f: fall-through
    rd_il_var(il, instr->address + instr->length);
}

void x86_lift(RDContext* ctx, const RDInstruction* instr, RDILInstruction* il,
              RDProcessor* p) {
    RD_UNUSED(p);

    switch(instr->id) {
        case ZYDIS_MNEMONIC_NOP: rd_il_nop(il); break;

        case ZYDIS_MNEMONIC_CALL:
            rd_il_call(il);
            _x86_lift_op(il, instr, 0);
            break;

        case ZYDIS_MNEMONIC_JMP:
            rd_il_goto(il);
            _x86_lift_op(il, instr, 0);
            break;

        case ZYDIS_MNEMONIC_MOV:
        case ZYDIS_MNEMONIC_LEA:
            rd_il_copy(il);
            _x86_lift_op(il, instr, 0);
            _x86_lift_op(il, instr, 1);
            break;

        case ZYDIS_MNEMONIC_PUSH:
            rd_il_push(il);
            _x86_lift_op(il, instr, 0);
            break;

        case ZYDIS_MNEMONIC_POP:
            rd_il_pop(il);
            _x86_lift_op(il, instr, 0);
            break;

        case ZYDIS_MNEMONIC_CMP:
            // z = left - right
            rd_il_copy(il);
            rd_il_sym(il, "z");
            rd_il_sub(il);
            _x86_lift_op(il, instr, 0);
            _x86_lift_op(il, instr, 1);
            break;

        case ZYDIS_MNEMONIC_ENTER:
            // push(ebp)
            rd_il_push(il);
            rd_il_reg(il, x86_get_bp(ctx));
            // copy(ebp, esp)
            rd_il_copy(il);
            rd_il_reg(il, x86_get_bp(ctx));
            rd_il_reg(il, x86_get_sp(ctx));
            break;

        case ZYDIS_MNEMONIC_LEAVE:
            // copy(esp, ebp)
            rd_il_copy(il);
            rd_il_reg(il, x86_get_sp(ctx));
            rd_il_reg(il, x86_get_bp(ctx));
            // pop(ebp)
            rd_il_pop(il);
            rd_il_reg(il, x86_get_bp(ctx));
            break;

        case ZYDIS_MNEMONIC_RET: {
            // pop(result)
            rd_il_pop(il);
            rd_il_sym(il, "result");

            // ret N: stdcall stack cleanup: copy(esp, add(esp, N))
            if(instr->operands[0].kind == RD_OP_IMM) {
                rd_il_copy(il);
                rd_il_reg(il, x86_get_sp(ctx));
                rd_il_add(il);
                rd_il_reg(il, x86_get_sp(ctx));
                rd_il_uint(il, instr->operands[0].imm);
            }

            rd_il_ret(il);
            rd_il_sym(il, "result");
            break;
        }

        case ZYDIS_MNEMONIC_ADD:
            rd_il_copy(il);
            _x86_lift_op(il, instr, 0);
            rd_il_add(il);
            _x86_lift_op(il, instr, 0);
            _x86_lift_op(il, instr, 1);
            break;

        case ZYDIS_MNEMONIC_SUB:
            rd_il_copy(il);
            _x86_lift_op(il, instr, 0);
            rd_il_sub(il);
            _x86_lift_op(il, instr, 0);
            _x86_lift_op(il, instr, 1);
            break;

        case ZYDIS_MNEMONIC_MUL:
            rd_il_copy(il);
            _x86_lift_op(il, instr, 0);
            rd_il_mul(il);
            _x86_lift_op(il, instr, 0);
            _x86_lift_op(il, instr, 1);
            break;

        case ZYDIS_MNEMONIC_DIV:
            rd_il_copy(il);
            _x86_lift_op(il, instr, 0);
            rd_il_div(il);
            _x86_lift_op(il, instr, 0);
            _x86_lift_op(il, instr, 1);
            break;

        case ZYDIS_MNEMONIC_AND:
            rd_il_copy(il);
            _x86_lift_op(il, instr, 0);
            rd_il_and(il);
            _x86_lift_op(il, instr, 0);
            _x86_lift_op(il, instr, 1);
            break;

        case ZYDIS_MNEMONIC_OR:
            rd_il_copy(il);
            _x86_lift_op(il, instr, 0);
            rd_il_or(il);
            _x86_lift_op(il, instr, 0);
            _x86_lift_op(il, instr, 1);
            break;

        case ZYDIS_MNEMONIC_XOR:
            rd_il_copy(il);
            _x86_lift_op(il, instr, 0);
            rd_il_xor(il);
            _x86_lift_op(il, instr, 0);
            _x86_lift_op(il, instr, 1);
            break;

        case ZYDIS_MNEMONIC_JZ:
        case ZYDIS_MNEMONIC_JNZ: _x86_lift_jump(il, instr); break;

        default: rd_il_unkn(il); break;
    }
}
