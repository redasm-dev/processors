#include "common.h"

#define ARM_OP_REGMASK (RD_OP_USERBASE + 0)

static int _capstone_arm32_reg_to_index(arm_reg reg) {
    switch(reg) {
        case ARM_REG_R0: return 0;
        case ARM_REG_R1: return 1;
        case ARM_REG_R2: return 2;
        case ARM_REG_R3: return 3;
        case ARM_REG_R4: return 4;
        case ARM_REG_R5: return 5;
        case ARM_REG_R6: return 6;
        case ARM_REG_R7: return 7;
        case ARM_REG_R8: return 8;
        case ARM_REG_SB: return 9;
        case ARM_REG_SL: return 10;
        case ARM_REG_FP: return 11;
        case ARM_REG_IP: return 12;
        case ARM_REG_SP: return 13;
        case ARM_REG_LR: return 14;
        case ARM_REG_PC: return 15;
        default: return -1;
    }
}

static arm_reg _capstone_arm32_index_to_reg(int idx) {
    switch(idx) {
        case 0: return ARM_REG_R0;
        case 1: return ARM_REG_R1;
        case 2: return ARM_REG_R2;
        case 3: return ARM_REG_R3;
        case 4: return ARM_REG_R4;
        case 5: return ARM_REG_R5;
        case 6: return ARM_REG_R6;
        case 7: return ARM_REG_R7;
        case 8: return ARM_REG_R8;
        case 9: return ARM_REG_SB;
        case 10: return ARM_REG_SL;
        case 11: return ARM_REG_FP;
        case 12: return ARM_REG_IP;
        case 13: return ARM_REG_SP;
        case 14: return ARM_REG_LR;
        case 15: return ARM_REG_PC;
        default: return ARM_REG_INVALID;
    }
}

void capstone_plugin_arm32_emulate(RDContext* ctx, const RDInstruction* instr,
                                   RDProcessor* p) {
    RD_UNUSED(p);

    rd_foreach_operand(i, op, instr) {
        if(op->kind == RD_OP_ADDR) {
            if(rd_is_call(instr))
                rd_add_xref(ctx, instr->address, op->addr, RD_CR_CALL);
            else if(rd_is_jump(instr))
                rd_add_xref(ctx, instr->address, op->addr, RD_CR_JUMP);
            else
                rd_add_xref(ctx, instr->address, op->addr, RD_DR_ADDRESS);
        }
        else if(op->kind == RD_OP_MEM && rd_is_address(ctx, op->mem)) {
            rd_add_xref(ctx, instr->address, op->mem, RD_DR_READ);

            const RDOperand* dst = &instr->operands[0];
            const RDOperand* src = &instr->operands[1];
            if(dst->kind == RD_OP_REG && src->kind == RD_OP_MEM)
                rd_auto_type(ctx, src->mem, "u32", 0, RD_TYPE_PTR);
        }
    }

    if(rd_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

bool capstone_plugin_arm32_render_operand(RDRenderer* r,
                                          const RDInstruction* instr, usize idx,
                                          RDProcessor* p) {
    RD_UNUSED(p);
    const RDOperand* op = &instr->operands[idx];

    if(op->kind == RD_OP_DISPL) {
        rd_renderer_norm(r, "[");
        rd_renderer_reg(r, op->displ.base);

        if(!op->userdata1 && op->displ.offset != 0) {
            rd_renderer_norm(r, ", #");
            rd_renderer_num(r, op->displ.offset, 10, 0, RD_NUM_SIGNED);
        }

        rd_renderer_norm(r, "]");

        if(op->userdata1) { // post-index
            if(op->displ.offset != 0) {
                rd_renderer_norm(r, ", #");
                rd_renderer_num(r, op->displ.offset, 10, 0, RD_NUM_SIGNED);
            }
        }
        else if(instr->write_back)
            rd_renderer_norm(r, "!");

        return true;
    }

    if(op->kind == RD_OP_PHRASE) {
        rd_renderer_norm(r, "[");
        rd_renderer_reg(r, op->phrase.base);
        rd_renderer_norm(r, ", ");
        rd_renderer_reg(r, op->phrase.index);
        rd_renderer_norm(r, "]");
        if(instr->write_back) rd_renderer_norm(r, "!");
        return true;
    }

    if(op->kind == ARM_OP_REGMASK) {
        rd_renderer_norm(r, "{");
        bool first = true;

        for(int i = 0; i < 16; i++) {
            if(!(op->imm & (1U << i))) continue;
            if(!first) rd_renderer_norm(r, ", ");

            rd_renderer_reg(r, _capstone_arm32_index_to_reg(i));
            first = false;
        }

        rd_renderer_norm(r, "}");
        return true;
    }

    return false;
}

bool capstone_arm32_decode_regmask(const cs_insn* cs_insn, RDInstruction* instr,
                                   usize idx) {
    const cs_arm* d = &cs_insn->detail->arm;
    RDOperand* op = &instr->operands[idx];
    op->kind = ARM_OP_REGMASK;
    op->imm = 0;

    for(uint8_t i = idx; i < d->op_count; i++) {
        if(d->operands[i].type == ARM_OP_REG) {
            int bit = _capstone_arm32_reg_to_index(d->operands[i].reg);
            if(bit >= 0) op->imm |= (1U << bit);
        }
    }

    return op->imm & (1U << 15);
}

bool capstone_arm32_decode_flow(const cs_insn* cs_insn, RDInstruction* instr) {
    const cs_arm* d = &cs_insn->detail->arm;

    switch(instr->id) {
        case ARM_INS_UDF: instr->flow = RD_IF_STOP; break;

        case ARM_INS_BX: {
            if(d->operands[0].reg == ARM_REG_LR)
                instr->flow = RD_IF_STOP;
            else
                instr->flow = (d->cc == ARMCC_AL || d->cc == ARMCC_Invalid)
                                  ? RD_IF_JUMP
                                  : RD_IF_JUMP_COND;
            break;
        }

        case ARM_INS_B: {
            instr->flow = (d->cc == ARMCC_AL || d->cc == ARMCC_Invalid)
                              ? RD_IF_JUMP
                              : RD_IF_JUMP_COND;
            break;
        }

        case ARM_INS_BL:
        case ARM_INS_BLX: {
            instr->flow = (d->cc == ARMCC_AL || d->cc == ARMCC_Invalid)
                              ? RD_IF_CALL
                              : RD_IF_CALL_COND;
            break;
        }

        case ARM_INS_STM:
        case ARM_INS_STMDA:
        case ARM_INS_STMDB:
        case ARM_INS_STMIB: {
            if(cs_insn->is_alias) {
                if(capstone_arm32_decode_regmask(cs_insn, instr, 0))
                    instr->flow = RD_IF_STOP;
            }
            else {
                instr->operands[0].kind = RD_OP_REG;
                instr->operands[0].reg = d->operands[0].reg;
                capstone_arm32_decode_regmask(cs_insn, instr, 1);
            }

            return true;
        }

        case ARM_INS_LDM:
        case ARM_INS_LDMDA:
        case ARM_INS_LDMDB:
        case ARM_INS_LDMIB: {
            if(cs_insn->is_alias) {
                if(capstone_arm32_decode_regmask(cs_insn, instr, 0))
                    instr->flow = RD_IF_STOP;
            }
            else {
                instr->operands[0].kind = RD_OP_REG;
                instr->operands[0].reg = d->operands[0].reg;

                if(capstone_arm32_decode_regmask(cs_insn, instr, 1))
                    instr->flow = RD_IF_STOP;
            }

            return true;
        }

        case ARM_INS_PUSH:
            capstone_arm32_decode_regmask(cs_insn, instr, 0);
            return true;

        case ARM_INS_POP: {
            if(capstone_arm32_decode_regmask(cs_insn, instr, 0))
                instr->flow = RD_IF_STOP;
            return true;
        }

        default: break;
    }

    return false;
}
