#include "decoder/decoder.h"
#include "decoder/registers.h"
#include <redasm/redasm.h>

static void z80_decode(RDContext* ctx, RDInstruction* instr, RDProcessor* p) {
    RD_UNUSED(p);

    Z80InstructionResult res;
    if(!z80_find_instruction(ctx, instr->address, &res)) return;
    if(!z80_decode_op(ctx, instr, 0, &res)) return;
    if(!z80_decode_op(ctx, instr, 1, &res)) return;

    // set instruction properties when decoding is completed
    // in this way "success" is reported to the core
    instr->id = res.instr.id;
    instr->length = res.instr.trailing + res.cursor;
    instr->flow = res.instr.flow;
}

static void z80_emulate(RDContext* ctx, const RDInstruction* instr,
                        RDProcessor* p) {
    RD_UNUSED(p);

    z80_track_regs(ctx, instr);
    z80_track_derefs(ctx, instr);

    rd_foreach_operand(i, op, instr) {
        if(op->kind == RD_OP_ADDR) {
            if(rd_instr_is_call(instr))
                rd_add_xref(ctx, instr->address, op->addr, RD_CR_CALL);
            else if(rd_instr_is_jump(instr))
                rd_add_xref(ctx, instr->address, op->addr, RD_CR_JUMP);
            else
                rd_add_xref(ctx, instr->address, op->addr, RD_DR_ADDRESS);
        }
        else if(op->kind == Z80_USEROP_IND_NN) {
            rd_add_xref(ctx, instr->address, op->imm,
                        (i == 0) ? RD_DR_WRITE : RD_DR_READ);
        }
    }

    if(rd_instr_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

static bool z80_render_operand(RDRenderer* r, const RDInstruction* instr,
                               int idx, RDProcessor* p) {
    RD_UNUSED(p);

    const RDOperand* op = &instr->operands[idx];

    switch(op->kind) {
        case RD_OP_IMM: {
            if(op->size == sizeof(u16) && op->imm != 0) {
                rd_renderer_loc(r, (RDAddress)op->imm, 2, RD_NUM_DEFAULT);
                return true;
            }

            return false;
        }

        case RD_OP_DISPL: {
            rd_renderer_norm(r, "(");
            rd_renderer_reg(r, op->displ.base);
            rd_renderer_num(r, op->displ.offset, 16, 0, RD_NUM_SIGNED);
            rd_renderer_norm(r, ")");
            return true;
        }

        case Z80_USEROP_CC: {
            rd_renderer_text(r, z80_cc_name((Z80Condition)op->cnst),
                             RD_THEME_REG, RD_THEME_DEFAULT);
            return true;
        }

        case Z80_USEROP_IND_N: {
            rd_renderer_norm(r, "(");
            rd_renderer_num(r, (i64)op->imm, 16, 1, RD_NUM_DEFAULT);
            rd_renderer_norm(r, ")");
            return true;
        }

        case Z80_USEROP_IND_NN: {
            rd_renderer_norm(r, "(");
            rd_renderer_loc(r, (i64)op->imm, 0, RD_NUM_DEFAULT);
            rd_renderer_norm(r, ")");
            return true;
        }

        case Z80_USEROP_IND_REG: {
            rd_renderer_norm(r, "(");
            rd_renderer_reg(r, op->reg);
            rd_renderer_norm(r, ")");
            return true;
        }

        case Z80_USEROP_IND_IDX_COPY: {
            rd_renderer_norm(r, "(");
            rd_renderer_reg(r, op->displ.base);
            rd_renderer_num(r, op->displ.offset, 16, 0, RD_NUM_SIGNED);
            rd_renderer_norm(r, ")");

            if(op->displ.index != Z80_REG_INVALID) {
                rd_renderer_norm(r, ", ");
                rd_renderer_reg(r, op->displ.index);
            }

            return true;
        }

        default: break;
    }

    return false;
}

static const RDProcessorPlugin Z80 = {
    .level = RD_API_LEVEL,
    .id = "z80",
    .name = "Zilog 80",
    .ptr_size = sizeof(u16),
    .get_mnemonic = z80_get_mnemonic,
    .query_reg = z80_query_reg,
    .decode = z80_decode,
    .emulate = z80_emulate,
    .render_operand = z80_render_operand,
};

void rd_plugin_create(void) { rd_register_processor(&Z80); }
