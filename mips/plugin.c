#include "formats.h"
#include "macros.h"
#include "registers.h"
#include <redasm/redasm.h>

static void _mips_handle_operands(RDContext* ctx, const RDInstruction* instr) {
    rd_foreach_operand(i, op, instr) {
        switch(op->kind) {
            case RD_OP_ADDR: {
                if(rd_is_call(instr))
                    rd_add_xref(ctx, instr->address, op->addr, RD_CR_CALL);
                else if(rd_is_jump(instr))
                    rd_add_xref(ctx, instr->address, op->addr, RD_CR_JUMP);
                break;
            };

            default: break;
        }
    }
}

static void _mips32_process_decoded(const RDContext* ctx,
                                    MIPSDecodedInstruction* dec,
                                    RDInstruction* instr) {
    mips_simplify(ctx, dec, instr);

    instr->id = dec->opcode->id;
    instr->length = dec->length;
    instr->uservalue1 = dec->opcode->format;

    if(mips_has_delay_slot(dec->opcode->id)) instr->delay_slots = 1;

    switch(dec->opcode->format) {
        case MIPS_FORMAT_R: mips_decode_r(dec, instr); break;
        case MIPS_FORMAT_I: mips_decode_i(dec, instr); break;
        case MIPS_FORMAT_J: mips_decode_j(dec, instr); break;
        case MIPS_FORMAT_B: mips_decode_b(dec, instr); break;
        case MIPS_FORMAT_MACRO: mips_decode_macro(dec, instr); break;
        default: break;
    }

    switch(dec->opcode->category) {
        case MIPS_CATEGORY_CALL: instr->flow = RD_IF_CALL; break;
        case MIPS_CATEGORY_RET: instr->flow = RD_IF_STOP; break;
        case MIPS_CATEGORY_JUMP_COND: instr->flow = RD_IF_JUMP_COND; break;
        case MIPS_CATEGORY_JUMP: instr->flow = RD_IF_JUMP; break;
        default: break;
    }
}

static void _mips32_decode_le(RDContext* ctx, RDInstruction* instr,
                              RDProcessor* p) {
    RD_UNUSED(p);
    MIPSDecodedInstruction dec = {0};
    if(!mips_decode_one_le(ctx, instr->address, &dec)) return;

    _mips32_process_decoded(ctx, &dec, instr);
}

static void _mips32_decode_be(RDContext* ctx, RDInstruction* instr,
                              RDProcessor* p) {
    RD_UNUSED(p);

    MIPSDecodedInstruction dec = {0};
    if(!mips_decode_one_be(ctx, instr->address, &dec)) return;

    _mips32_process_decoded(ctx, &dec, instr);
}

static const char* _mips32_get_mnemonic(const RDInstruction* instr,
                                        RDProcessor* p) {
    RD_UNUSED(p);
    return mips_get_mnemonic(instr->id);
}

static const char* _mips32_get_register(int r, RDProcessor* p) {
    RD_UNUSED(p);
    return mips_get_register(r);
}

static void _mips32_emulate(RDContext* ctx, const RDInstruction* instr,
                            RDProcessor* p) {
    RD_UNUSED(p);
    switch(instr->id) {
        case MIPS_MACRO_LA: {
            rd_add_xref(ctx, instr->address, instr->operands[1].addr,
                        RD_DR_ADDRESS);
            break;
        }

        case MIPS_MACRO_LW: {
            rd_add_xref(ctx, instr->address, instr->operands[1].addr,
                        RD_DR_READ);
            break;
        }

        case MIPS_MACRO_SW:
        case MIPS_MACRO_SH: {
            rd_add_xref(ctx, instr->address, instr->operands[1].addr,
                        RD_DR_WRITE);
            break;
        }

        default: _mips_handle_operands(ctx, instr); break;
    }

    if(rd_is_delay_slot(instr)) {
        RDDelaySlotInfo dslot = rd_get_delay_slot_info(ctx);

        if(dslot.n == dslot.instr.delay_slots && dslot.instr.flow == RD_IF_STOP)
            return;
    }

    if(rd_can_flow(instr) || instr->delay_slots)
        rd_flow(ctx, instr->address + instr->length);
}

static void _mips32_render_mnemonic(RDRenderer* r, const RDInstruction* instr,
                                    RDProcessor* p) {
    RD_UNUSED(p);

    switch(instr->id) {
        case MIPS_MACRO_NOP: rd_renderer_mnem(r, instr, RD_THEME_MUTED); return;
        case MIPS_MACRO_B: rd_renderer_mnem(r, instr, RD_THEME_JUMP); return;
        default: break;
    }

    switch(instr->flow) {
        case RD_IF_JUMP: rd_renderer_mnem(r, instr, RD_THEME_JUMP); break;

        case RD_IF_JUMP_COND:
            rd_renderer_mnem(r, instr, RD_THEME_JUMP_COND);
            break;

        case RD_IF_CALL: rd_renderer_mnem(r, instr, RD_THEME_CALL); break;

        case RD_IF_CALL_COND:
            rd_renderer_mnem(r, instr, RD_THEME_CALL_COND);
            break;

        case RD_IF_STOP: rd_renderer_mnem(r, instr, RD_THEME_STOP); break;
        default: rd_renderer_mnem(r, instr, RD_THEME_DEFAULT); break;
    }
}

static void _mips32_render_operand(RDRenderer* r, const RDInstruction* instr,
                                   usize idx, RDProcessor* p) {
    RD_UNUSED(p);
    const RDOperand* op = &instr->operands[idx];

    switch(op->kind) {
        case RD_OP_REG: rd_renderer_reg(r, op->reg); break;
        case RD_OP_IMM: rd_renderer_cnst(r, op->imm, 16, 0, 0); break;
        case RD_OP_ADDR: rd_renderer_loc(r, op->addr, 0, 0); break;
        case RD_OP_MEM: rd_renderer_loc(r, op->mem, 0, 0); break;

        case RD_OP_DISPL:
            rd_renderer_cnst(r, op->displ.displ, 16, 0, 0);
            rd_renderer_norm(r, "(");
            rd_renderer_reg(r, op->displ.base);
            rd_renderer_norm(r, ")");
            break;

        default: break;
    }
}

static const RDProcessorPlugin MIPS32_BE = {
    .level = RD_API_LEVEL,
    .id = "mips32_be",
    .name = "MIPS32 (Big Endian)",
    .flags = RD_PF_BE,
    .ptr_size = sizeof(u32),
    .int_size = sizeof(u32),
    .get_mnemonic = _mips32_get_mnemonic,
    .get_register = _mips32_get_register,
    .decode = _mips32_decode_be,
    .emulate = _mips32_emulate,
    .render_mnemonic = _mips32_render_mnemonic,
    .render_operand = _mips32_render_operand,
};

static const RDProcessorPlugin MIPS32_LE = {
    .level = RD_API_LEVEL,
    .id = "mips32_le",
    .name = "MIPS32 (Little Endian)",
    .flags = RD_PF_LE,
    .ptr_size = sizeof(u32),
    .int_size = sizeof(u32),
    .get_mnemonic = _mips32_get_mnemonic,
    .get_register = _mips32_get_register,
    .decode = _mips32_decode_le,
    .emulate = _mips32_emulate,
    .render_mnemonic = _mips32_render_mnemonic,
    .render_operand = _mips32_render_operand,
};

void rd_plugin_create(void) {
    mips_initialize_formats();
    rd_register_processor(&MIPS32_BE);
    rd_register_processor(&MIPS32_LE);
}
