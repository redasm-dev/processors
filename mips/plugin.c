#include "decoder/formats.h"
#include "decoder/macros.h"
#include "decoder/registers.h"
#include "lifter.h"
#include "registers.h"
#include <redasm/redasm.h>

static void _mips_handle_operands(RDContext* ctx, const RDInstruction* instr) {
    rd_foreach_operand(i, op, instr) {
        switch(op->kind) {
            case RD_OP_ADDR: {
                if(rd_instr_is_call(instr))
                    rd_add_xref(ctx, instr->address, op->addr, RD_CR_CALL);
                else if(rd_instr_is_jump(instr))
                    rd_add_xref(ctx, instr->address, op->addr, RD_CR_JUMP);
                break;
            };

            default: break;
        }
    }
}

static void _mips32_process_decoded(MIPSDecodedInstruction* dec,
                                    RDInstruction* instr) {
    mips_simplify(dec);

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

static void mips32_decode_le(RDContext* ctx, RDInstruction* instr,
                             RDProcessor* p) {
    RD_UNUSED(p);
    MIPSDecodedInstruction dec = {0};
    if(!mips_decode_le(ctx, instr->address, &dec)) return;

    _mips32_process_decoded(&dec, instr);
}

static void mips32_decode_be(RDContext* ctx, RDInstruction* instr,
                             RDProcessor* p) {
    RD_UNUSED(p);

    MIPSDecodedInstruction dec = {0};
    if(!mips_decode_be(ctx, instr->address, &dec)) return;

    _mips32_process_decoded(&dec, instr);
}

static const char* mips32_get_mnemonic(const RDInstruction* instr,
                                       RDProcessor* p) {
    RD_UNUSED(p);
    return mips_get_mnemonic(instr->id);
}

static bool mips32_query_reg(RDQueryReg* q, RDProcessor* p) {
    RD_UNUSED(p);

    if(q->kind == RD_QUERY_REG_BY_ID) {
        q->name = mips_get_register_name(q->id);
        if(!q->name) return false;
    }
    else if(q->kind == RD_QUERY_REG_BY_NAME) {
        q->id = mips_get_register_id(q->name);
        if(q->id == RD_REGID_INVALID) return false;
    }
    else
        return false;

    if(q->want & RD_QUERY_REG_WANT_CANONICAL) q->canonical_name = q->name;

    return true;
}

static void mips32_emulate(RDContext* ctx, const RDInstruction* instr,
                           RDProcessor* p) {
    RD_UNUSED(p);

    RDAddress next = rd_instr_is_delay_slot(instr)
                         ? instr->address
                         : instr->address + instr->length;

    switch(instr->id) {
        case MIPS_MACRO_LW:
        case MIPS_MACRO_LHU: {
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

        case MIPS_MACRO_LI: {
            // addiu rt, $zero, imm: zero-extended 16-bit immediate
            mips_set_regval(ctx, next, instr->operands[0].reg,
                            (RDRegValue)(u16)instr->operands[1].imm);
            break;
        }

        case MIPS_MACRO_MOVE: {
            RDRegValue val;
            if(mips_get_regval(ctx, instr->address, instr->operands[1].reg,
                               &val))
                mips_set_regval(ctx, next, instr->operands[0].reg, val);
            else
                mips_del_regval(ctx, next, instr->operands[0].reg);

            break;
        }

        case MIPS_INSTR_LUI: {
            RDRegValue val = ((u32)instr->operands[1].imm << 16);
            mips_set_regval(ctx, next, instr->operands[0].reg, val);
            break;
        }

        case MIPS_INSTR_ADDI:
        case MIPS_INSTR_ADDIU: {
            RDRegValue val;

            if(mips_get_regval(ctx, instr->address, instr->operands[1].reg,
                               &val)) {
                RDRegValue result =
                    (u32)((i32)val + (i16)instr->operands[2].imm);
                mips_set_regval(ctx, next, instr->operands[0].reg, result);
                if(rd_is_address(ctx, result))
                    rd_add_xref(ctx, instr->address, result, RD_DR_ADDRESS);
            }
            else
                mips_del_regval(ctx, next, instr->operands[0].reg);

            break;
        }

        case MIPS_INSTR_ORI: {
            RDRegValue val;

            if(mips_get_regval(ctx, instr->address, instr->operands[1].reg,
                               &val)) {
                RDRegValue result = ((u32)val | (u16)instr->operands[2].imm);
                mips_set_regval(ctx, next, instr->operands[0].reg, result);
                if(rd_is_address(ctx, result))
                    rd_add_xref(ctx, instr->address, result, RD_DR_ADDRESS);
            }
            else
                mips_del_regval(ctx, next, instr->operands[0].reg);

            break;
        }

        // register relative loads: raw I-format displ, not macro-fused
        case MIPS_INSTR_LW:
        case MIPS_INSTR_LH:
        case MIPS_INSTR_LBU:
        case MIPS_INSTR_LB:
        case MIPS_INSTR_LHU: {
            if(instr->operands[1].kind != RD_OP_DISPL) break;

            RDRegValue base;
            if(!mips_get_regval(ctx, instr->address,
                                instr->operands[1].displ.base, &base)) {
                break;
            }

            RDAddress ea = ((i32)base + (i16)instr->operands[1].displ.offset);
            rd_add_xref(ctx, instr->address, ea, RD_DR_READ);
            break;
        }

        case MIPS_INSTR_SW:
        case MIPS_INSTR_SH:
        case MIPS_INSTR_SB: {
            if(instr->operands[1].kind != RD_OP_DISPL) break;

            RDRegValue base;
            if(!mips_get_regval(ctx, instr->address,
                                instr->operands[1].displ.base, &base)) {
                break;
            }

            RDAddress ea = ((i32)base + (i16)instr->operands[1].displ.offset);
            rd_add_xref(ctx, instr->address, ea, RD_DR_WRITE);
            break;
        }
        default: _mips_handle_operands(ctx, instr); break;
    }

    if(rd_instr_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

static bool mips32_render_mnemonic(RDRenderer* r, const RDInstruction* instr,
                                   RDProcessor* p) {
    RD_UNUSED(p);

    switch(instr->id) {
        case MIPS_MACRO_NOP:
            rd_renderer_mnem(r, instr, RD_THEME_MUTED);
            return true;

        case MIPS_MACRO_B:
            rd_renderer_mnem(r, instr, RD_THEME_JUMP);
            return true;

        default: break;
    }

    return false;
}

static bool mips32_render_operan(RDRenderer* r, const RDInstruction* instr,
                                 int idx, RDProcessor* p) {
    RD_UNUSED(p);
    const RDOperand* op = &instr->operands[idx];

    if(op->kind == RD_OP_DISPL) {
        rd_renderer_num(r, op->displ.offset, 16, 0, 0);
        rd_renderer_norm(r, "(");
        rd_renderer_reg(r, op->displ.base);
        rd_renderer_norm(r, ")");
        return true;
    }

    return false;
}

static const RDProcessorPlugin MIPS32_BE = {
    .level = RD_API_LEVEL,
    .id = "mips32_be",
    .name = "MIPS32 (Big Endian)",
    .flags = RD_PF_BE,
    .ptr_size = sizeof(u32),
    .get_mnemonic = mips32_get_mnemonic,
    .query_reg = mips32_query_reg,
    .decode = mips32_decode_be,
    .emulate = mips32_emulate,
    .lift = mips32_lift,
    .render_mnemonic = mips32_render_mnemonic,
    .render_operand = mips32_render_operan,
};

static const RDProcessorPlugin MIPS32_LE = {
    .level = RD_API_LEVEL,
    .id = "mips32_le",
    .name = "MIPS32 (Little Endian)",
    .flags = RD_PF_LE,
    .ptr_size = sizeof(u32),
    .get_mnemonic = mips32_get_mnemonic,
    .query_reg = mips32_query_reg,
    .decode = mips32_decode_le,
    .emulate = mips32_emulate,
    .lift = mips32_lift,
    .render_mnemonic = mips32_render_mnemonic,
    .render_operand = mips32_render_operan,
};

void rd_plugin_create(void) {
    mips_initialize_formats();
    rd_register_processor(&MIPS32_BE);
    rd_register_processor(&MIPS32_LE);
}

const char* rd_plugin_version(void) { return "1.0"; }
