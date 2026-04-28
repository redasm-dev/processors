#include "macros.h"
#include "registers.h"

// clang-format off
static const MIPSOpcode M_OPCODE_NOP   = {MIPS_MACRO_NOP,  MIPS_CATEGORY_NONE,      MIPS_FORMAT_MACRO, MIPS_VERSION_NONE};
static const MIPSOpcode M_OPCODE_MOVE  = {MIPS_MACRO_MOVE, MIPS_CATEGORY_NONE,      MIPS_FORMAT_MACRO, MIPS_VERSION_NONE};
static const MIPSOpcode M_OPCODE_LI    = {MIPS_MACRO_LI,   MIPS_CATEGORY_NONE,      MIPS_FORMAT_MACRO, MIPS_VERSION_NONE};
static const MIPSOpcode M_OPCODE_B     = {MIPS_MACRO_B,    MIPS_CATEGORY_JUMP,      MIPS_FORMAT_MACRO, MIPS_VERSION_NONE};
static const MIPSOpcode M_OPCODE_BEQZ  = {MIPS_MACRO_BEQZ, MIPS_CATEGORY_JUMP_COND, MIPS_FORMAT_MACRO, MIPS_VERSION_NONE};
static const MIPSOpcode M_OPCODE_BNEZ  = {MIPS_MACRO_BNEZ, MIPS_CATEGORY_JUMP_COND, MIPS_FORMAT_MACRO, MIPS_VERSION_NONE};
static const MIPSOpcode M_OPCODE_LA    = {MIPS_MACRO_LA,   MIPS_CATEGORY_NONE,      MIPS_FORMAT_MACRO, MIPS_VERSION_NONE};
static const MIPSOpcode M_OPCODE_LW    = {MIPS_MACRO_LW,   MIPS_CATEGORY_LOAD,      MIPS_FORMAT_MACRO, MIPS_VERSION_NONE};
static const MIPSOpcode M_OPCODE_LHU   = {MIPS_MACRO_LHU,  MIPS_CATEGORY_LOAD,      MIPS_FORMAT_MACRO, MIPS_VERSION_NONE};
static const MIPSOpcode M_OPCODE_SW    = {MIPS_MACRO_SW,   MIPS_CATEGORY_STORE,     MIPS_FORMAT_MACRO, MIPS_VERSION_NONE};
static const MIPSOpcode M_OPCODE_SH    = {MIPS_MACRO_SH,   MIPS_CATEGORY_STORE,     MIPS_FORMAT_MACRO, MIPS_VERSION_NONE};
static const MIPSOpcode M_OPCODE_RET   = {MIPS_INSTR_JR,   MIPS_CATEGORY_RET,       MIPS_FORMAT_R,     MIPS_VERSION_NONE};
// clang-format on

static bool _mips_can_simplify_lui(const MIPSDecodedInstruction* lui,
                                   const MIPSDecodedInstruction* next) {
    switch(next->opcode->format) {
        case MIPS_FORMAT_I: return lui->instr.i_u.rt == next->instr.i_u.rs;

        case MIPS_FORMAT_R: {
            if(next->instr.r.rd != next->instr.r.rs) return false;
            return (lui->instr.i_u.rt == MIPS_REG_AT) &&
                   (next->instr.r.rd == MIPS_REG_AT);
        }

        default: break;
    }

    return false;
}

static void _mips_patch_lui(const RDContext* ctx, MIPSDecodedInstruction* dec,
                            RDAddress address) {
    const RDProcessorPlugin* p = rd_get_processor_plugin(ctx);
    RDAddress nextaddress = address + dec->length;
    MIPSDecodedInstruction next = {0};
    bool ok;

    if(p->flags & RD_PROCESSOR_BE)
        ok = mips_decode_one_be(ctx, nextaddress, &next);
    else
        ok = mips_decode_one_le(ctx, nextaddress, &next);

    if(!ok || !_mips_can_simplify_lui(dec, &next)) return;

    u32 mipsaddress = (u32)dec->instr.i_u.imm << 16;

    const MIPSOpcode* mop = NULL;

    switch(next.opcode->id) {
        case MIPS_INSTR_ORI:
            mipsaddress |= (u32)next.instr.i_u.imm; // sign extend
            mop = &M_OPCODE_LA;
            break;

        case MIPS_INSTR_ADDIU:
            mipsaddress += (i32)next.instr.i_s.imm; // sign extend
            mop = &M_OPCODE_LA;
            break;

        case MIPS_INSTR_LW:
            mipsaddress += (i32)next.instr.i_s.imm; // sign extend
            mop = &M_OPCODE_LW;
            break;

        case MIPS_INSTR_LHU:
            mipsaddress += (i32)next.instr.i_s.imm; // sign extend
            mop = &M_OPCODE_LHU;
            break;

        case MIPS_INSTR_SW:
            mipsaddress += (i32)next.instr.i_s.imm; // sign extend
            mop = &M_OPCODE_SW;
            break;

        case MIPS_INSTR_SH:
            mipsaddress += (i32)next.instr.i_s.imm; // sign extend
            mop = &M_OPCODE_SH;
            break;

        default: return;
    }

    // patch instruction to macro
    dec->opcode = mop;
    dec->length = sizeof(MIPSInstruction) * 2;
    dec->macro.regimm.reg = next.instr.i_u.rt;
    dec->macro.regimm.address = mipsaddress;
}

void mips_simplify(const RDContext* ctx, MIPSDecodedInstruction* dec,
                   const RDInstruction* instr) {
    switch(dec->opcode->id) {
        case MIPS_INSTR_ORI:
        case MIPS_INSTR_ADDI:
        case MIPS_INSTR_ADDIU: {
            if(dec->instr.i_u.rs != MIPS_REG_ZERO) {
                dec->macro.regimm.reg = dec->instr.i_u.rt;
                dec->macro.regimm.u_imm16 = dec->instr.i_u.imm;
                dec->opcode = &M_OPCODE_LI;
            }
            break;
        }

        case MIPS_INSTR_ADDU: {
            if(dec->instr.r.rt == MIPS_REG_ZERO) {
                dec->macro.regs.dst = dec->instr.r.rd;
                dec->macro.regs.src = dec->instr.r.rs;
                dec->opcode = &M_OPCODE_MOVE;
            }
            break;
        }

        case MIPS_INSTR_SLL: {
            if((dec->instr.r.rd == MIPS_REG_ZERO) &&
               (dec->instr.r.rt == MIPS_REG_ZERO))
                dec->opcode = &M_OPCODE_NOP;
            break;
        }

        case MIPS_INSTR_BEQ: {
            if(dec->instr.i_s.rt == dec->instr.i_s.rs) {
                dec->macro.regimm.s_imm16 = dec->instr.i_s.imm;
                dec->opcode = &M_OPCODE_B;
            }
            else if(dec->instr.i_s.rt == MIPS_REG_ZERO ||
                    dec->instr.i_s.rs == MIPS_REG_ZERO) {
                if(dec->instr.i_s.rt != MIPS_REG_ZERO)
                    dec->macro.regimm.reg = dec->instr.i_s.rt;
                else if(MIPS_REG_ZERO != dec->instr.i_s.rs)
                    dec->macro.regimm.reg = dec->instr.i_s.rs;

                dec->macro.regimm.s_imm16 = dec->instr.i_s.imm;
                dec->opcode = &M_OPCODE_BEQZ;
            }

            break;
        }

        case MIPS_INSTR_BNE: {
            if(dec->instr.i_s.rt == MIPS_REG_ZERO ||
               dec->instr.i_s.rs == MIPS_REG_ZERO) {
                if(dec->instr.i_s.rt != MIPS_REG_ZERO)
                    dec->macro.regimm.reg = dec->instr.i_s.rt;
                else if(dec->instr.i_s.rs != MIPS_REG_ZERO)
                    dec->macro.regimm.reg = dec->instr.i_s.rs;

                dec->macro.regimm.s_imm16 = dec->instr.i_s.imm;
                dec->opcode = &M_OPCODE_BNEZ;
            }
            break;
        }

        case MIPS_INSTR_JR: {
            if(dec->instr.r.rs == MIPS_REG_RA) dec->opcode = &M_OPCODE_RET;
            break;
        }

        case MIPS_INSTR_LUI: {
            if(!rd_is_delay_slot(instr))
                _mips_patch_lui(ctx, dec, instr->address);
            break;
        }

        default: break;
    }
}

void mips_decode_macro(const MIPSDecodedInstruction* dec,
                       RDInstruction* instr) {

    switch(dec->opcode->id) {
        case MIPS_MACRO_NOP: break;

        case MIPS_MACRO_MOVE: {
            instr->operands[0].kind = RD_OP_REG;
            instr->operands[0].reg = dec->macro.regs.dst;
            instr->operands[1].kind = RD_OP_REG;
            instr->operands[1].reg = dec->macro.regs.src;
            break;
        }

        case MIPS_MACRO_LI: {
            instr->operands[0].kind = RD_OP_REG;
            instr->operands[0].reg = dec->macro.regimm.reg;
            instr->operands[1].kind = RD_OP_IMM;
            instr->operands[1].imm = dec->macro.regimm.u_imm16;
            break;
        }

        case MIPS_MACRO_B: {
            instr->operands[0].kind = RD_OP_ADDR;
            instr->operands[0].addr =
                mips_calc_addr16(instr->address, dec->macro.regimm.s_imm16);
            break;
        }

        case MIPS_MACRO_BEQZ:
        case MIPS_MACRO_BNEZ: {
            instr->operands[0].kind = RD_OP_REG;
            instr->operands[0].reg = dec->macro.regimm.reg;
            instr->operands[1].kind = RD_OP_ADDR;
            instr->operands[1].addr =
                mips_calc_addr16(instr->address, dec->macro.regimm.s_imm16);
            break;
        }

        case MIPS_MACRO_LA: {
            instr->operands[0].kind = RD_OP_REG;
            instr->operands[0].reg = dec->macro.regimm.reg;
            instr->operands[1].kind = RD_OP_ADDR;
            instr->operands[1].addr = dec->macro.regimm.address;
            break;
        }

        case MIPS_MACRO_LW:
        case MIPS_MACRO_SW:
        case MIPS_MACRO_LHU:
        case MIPS_MACRO_SH: {
            instr->operands[0].kind = RD_OP_REG;
            instr->operands[0].reg = dec->macro.regimm.reg;
            instr->operands[1].kind = RD_OP_MEM;
            instr->operands[1].mem = dec->macro.regimm.address;
            break;
        }

        default: break;
    }
}
