#include "formats.h"

MIPSOpcode mips_opcodes_r[MIPS_N_OPCODES];
MIPSOpcode mips_opcodes_c[MIPS_N_OPCODES];
MIPSOpcode mips_opcodes_i[MIPS_N_OPCODES];
MIPSOpcode mips_opcodes_j[MIPS_N_OPCODES];
MIPSOpcode mips_opcodes_b[MIPS_N_OPCODES];
MIPSOpcode mips_opcodes_c0[MIPS_N_OPCODES];
MIPSOpcode mips_opcodes_c1[MIPS_N_OPCODES];
MIPSOpcode mips_opcodes_c2[MIPS_N_OPCODES];
MIPSOpcode mips_opcodes_cls[MIPS_N_OPCODES];

void mips_initialize_formats(void) {
    // clang-format off
    mips_opcodes_r[0x20] = (MIPSOpcode){MIPS_INSTR_ADD,   MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 100000
    mips_opcodes_r[0x21] = (MIPSOpcode){MIPS_INSTR_ADDU,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 100001
    mips_opcodes_r[0x24] = (MIPSOpcode){MIPS_INSTR_AND,   MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 100100
    mips_opcodes_r[0x1A] = (MIPSOpcode){MIPS_INSTR_DIV,   MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 011010
    mips_opcodes_r[0x1B] = (MIPSOpcode){MIPS_INSTR_DIVU,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 011011
    mips_opcodes_r[0x18] = (MIPSOpcode){MIPS_INSTR_MULT,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 011000
    mips_opcodes_r[0x19] = (MIPSOpcode){MIPS_INSTR_MULTU, MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 011001
    mips_opcodes_r[0x27] = (MIPSOpcode){MIPS_INSTR_NOR,   MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 100111
    mips_opcodes_r[0x25] = (MIPSOpcode){MIPS_INSTR_OR,    MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 100101
    mips_opcodes_r[0x00] = (MIPSOpcode){MIPS_INSTR_SLL,   MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 000000
    mips_opcodes_r[0x03] = (MIPSOpcode){MIPS_INSTR_SRA,   MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 000011
    mips_opcodes_r[0x02] = (MIPSOpcode){MIPS_INSTR_SRL,   MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 000010
    mips_opcodes_r[0x22] = (MIPSOpcode){MIPS_INSTR_SUB,   MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 100010
    mips_opcodes_r[0x23] = (MIPSOpcode){MIPS_INSTR_SUBU,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 100011
    mips_opcodes_r[0x26] = (MIPSOpcode){MIPS_INSTR_XOR,   MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 100110
    mips_opcodes_r[0x2A] = (MIPSOpcode){MIPS_INSTR_SLT,   MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 101010
    mips_opcodes_r[0x2B] = (MIPSOpcode){MIPS_INSTR_SLTU,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 101011
    mips_opcodes_r[0x08] = (MIPSOpcode){MIPS_INSTR_JR,    MIPS_CATEGORY_JUMP, MIPS_FORMAT_R, MIPS_V1 }; // 001000
    mips_opcodes_r[0x10] = (MIPSOpcode){MIPS_INSTR_MFHI,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 010000
    mips_opcodes_r[0x12] = (MIPSOpcode){MIPS_INSTR_MFLO,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 010010
    mips_opcodes_r[0x11] = (MIPSOpcode){MIPS_INSTR_MTHI,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 010001
    mips_opcodes_r[0x13] = (MIPSOpcode){MIPS_INSTR_MTLO,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 010011
    mips_opcodes_r[0x04] = (MIPSOpcode){MIPS_INSTR_SLLV,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 000100
    mips_opcodes_r[0x07] = (MIPSOpcode){MIPS_INSTR_SRAV,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 000111
    mips_opcodes_r[0x06] = (MIPSOpcode){MIPS_INSTR_SRLV,  MIPS_CATEGORY_NONE, MIPS_FORMAT_R, MIPS_V1 }; // 000110
    mips_opcodes_r[0x09] = (MIPSOpcode){MIPS_INSTR_JALR,  MIPS_CATEGORY_CALL, MIPS_FORMAT_R, MIPS_V1 }; // 001001

    mips_opcodes_c[0x30] = (MIPSOpcode){MIPS_INSTR_TGE,   MIPS_CATEGORY_NONE, MIPS_FORMAT_C, MIPS_V1 }; // 110000
    mips_opcodes_c[0x34] = (MIPSOpcode){MIPS_INSTR_TEQ,   MIPS_CATEGORY_NONE, MIPS_FORMAT_C, MIPS_V1 }; // 110100

    mips_opcodes_i[0x08] = (MIPSOpcode){MIPS_INSTR_ADDI,  MIPS_CATEGORY_NONE,      MIPS_FORMAT_I, MIPS_V1 }; // 001000
    mips_opcodes_i[0x09] = (MIPSOpcode){MIPS_INSTR_ADDIU, MIPS_CATEGORY_NONE,      MIPS_FORMAT_I, MIPS_V1 }; // 001001
    mips_opcodes_i[0x0C] = (MIPSOpcode){MIPS_INSTR_ANDI,  MIPS_CATEGORY_NONE,      MIPS_FORMAT_I, MIPS_V1 }; // 001100
    mips_opcodes_i[0x0D] = (MIPSOpcode){MIPS_INSTR_ORI,   MIPS_CATEGORY_NONE,      MIPS_FORMAT_I, MIPS_V1 }; // 001101
    mips_opcodes_i[0x0F] = (MIPSOpcode){MIPS_INSTR_LUI,   MIPS_CATEGORY_LOAD,      MIPS_FORMAT_I, MIPS_V1 }; // 001111
    mips_opcodes_i[0x04] = (MIPSOpcode){MIPS_INSTR_BEQ,   MIPS_CATEGORY_JUMP_COND, MIPS_FORMAT_I, MIPS_V1 }; // 000100
    mips_opcodes_i[0x05] = (MIPSOpcode){MIPS_INSTR_BNE,   MIPS_CATEGORY_JUMP_COND, MIPS_FORMAT_I, MIPS_V1 }; // 000101
    mips_opcodes_i[0x01] = (MIPSOpcode){MIPS_INSTR_BGEZ,  MIPS_CATEGORY_JUMP_COND, MIPS_FORMAT_I, MIPS_V1 }; // 000001
    mips_opcodes_i[0x07] = (MIPSOpcode){MIPS_INSTR_BGTZ,  MIPS_CATEGORY_JUMP_COND, MIPS_FORMAT_I, MIPS_V1 }; // 000111
    mips_opcodes_i[0x06] = (MIPSOpcode){MIPS_INSTR_BLEZ,  MIPS_CATEGORY_JUMP_COND, MIPS_FORMAT_I, MIPS_V1 }; // 000110
    mips_opcodes_i[0x20] = (MIPSOpcode){MIPS_INSTR_LB,    MIPS_CATEGORY_LOAD,      MIPS_FORMAT_I, MIPS_V1 }; // 100000
    mips_opcodes_i[0x24] = (MIPSOpcode){MIPS_INSTR_LBU,   MIPS_CATEGORY_LOAD,      MIPS_FORMAT_I, MIPS_V1 }; // 100100
    mips_opcodes_i[0x21] = (MIPSOpcode){MIPS_INSTR_LH,    MIPS_CATEGORY_LOAD,      MIPS_FORMAT_I, MIPS_V1 }; // 100001
    mips_opcodes_i[0x25] = (MIPSOpcode){MIPS_INSTR_LHU,   MIPS_CATEGORY_LOAD,      MIPS_FORMAT_I, MIPS_V1 }; // 100101
    mips_opcodes_i[0x23] = (MIPSOpcode){MIPS_INSTR_LW,    MIPS_CATEGORY_LOAD,      MIPS_FORMAT_I, MIPS_V1 }; // 100011
    mips_opcodes_i[0x22] = (MIPSOpcode){MIPS_INSTR_LWL,   MIPS_CATEGORY_LOAD,      MIPS_FORMAT_I, MIPS_V1 }; // 100010
    mips_opcodes_i[0x26] = (MIPSOpcode){MIPS_INSTR_LWR,   MIPS_CATEGORY_LOAD,      MIPS_FORMAT_I, MIPS_V1 }; // 100110
    mips_opcodes_i[0x28] = (MIPSOpcode){MIPS_INSTR_SB,    MIPS_CATEGORY_STORE,     MIPS_FORMAT_I, MIPS_V1 }; // 101000
    mips_opcodes_i[0x29] = (MIPSOpcode){MIPS_INSTR_SH,    MIPS_CATEGORY_STORE,     MIPS_FORMAT_I, MIPS_V1 }; // 101001
    mips_opcodes_i[0x2B] = (MIPSOpcode){MIPS_INSTR_SW,    MIPS_CATEGORY_STORE,     MIPS_FORMAT_I, MIPS_V1 }; // 101011
    mips_opcodes_i[0x2A] = (MIPSOpcode){MIPS_INSTR_SWL,   MIPS_CATEGORY_STORE,     MIPS_FORMAT_I, MIPS_V1 }; // 101010
    mips_opcodes_i[0x2E] = (MIPSOpcode){MIPS_INSTR_SWR,   MIPS_CATEGORY_STORE,     MIPS_FORMAT_I, MIPS_V1 }; // 101110
    mips_opcodes_i[0x19] = (MIPSOpcode){MIPS_INSTR_LHI,   MIPS_CATEGORY_NONE,      MIPS_FORMAT_I, MIPS_V1 }; // 011001
    mips_opcodes_i[0x18] = (MIPSOpcode){MIPS_INSTR_LLO,   MIPS_CATEGORY_NONE,      MIPS_FORMAT_I, MIPS_V1 }; // 011000
    mips_opcodes_i[0x0A] = (MIPSOpcode){MIPS_INSTR_SLTI,  MIPS_CATEGORY_NONE,      MIPS_FORMAT_I, MIPS_V1 }; // 001010
    mips_opcodes_i[0x0B] = (MIPSOpcode){MIPS_INSTR_SLTIU, MIPS_CATEGORY_NONE,      MIPS_FORMAT_I, MIPS_V1 }; // 001011
    mips_opcodes_i[0x0E] = (MIPSOpcode){MIPS_INSTR_XORI,  MIPS_CATEGORY_NONE,      MIPS_FORMAT_I, MIPS_V1 }; // 001110

    mips_opcodes_j[0x02] = (MIPSOpcode){MIPS_INSTR_J,     MIPS_CATEGORY_JUMP, MIPS_FORMAT_J, MIPS_V1 }; // 000010
    mips_opcodes_j[0x03] = (MIPSOpcode){MIPS_INSTR_JAL,   MIPS_CATEGORY_CALL, MIPS_FORMAT_J, MIPS_V1 }; // 000011

    mips_opcodes_b[0x0C] = (MIPSOpcode){MIPS_INSTR_SYSCALL, MIPS_CATEGORY_NONE, MIPS_FORMAT_B, MIPS_V1 }; // 001100
    mips_opcodes_b[0x0D] = (MIPSOpcode){MIPS_INSTR_BREAK,   MIPS_CATEGORY_NONE, MIPS_FORMAT_B, MIPS_V1 }; // 001101

    mips_opcodes_c0[0x00] = (MIPSOpcode){MIPS_INSTR_MFC0, MIPS_CATEGORY_LOAD, MIPS_FORMAT_C0, MIPS_V1 }; // 00000
    mips_opcodes_c0[0x04] = (MIPSOpcode){MIPS_INSTR_MTC0, MIPS_CATEGORY_LOAD, MIPS_FORMAT_C0, MIPS_V1 }; // 00100

    mips_opcodes_c2[0x00] = (MIPSOpcode){MIPS_INSTR_MFC2, MIPS_CATEGORY_NONE, MIPS_FORMAT_C2, MIPS_V1 }; // 00000
    mips_opcodes_c2[0x04] = (MIPSOpcode){MIPS_INSTR_MTC2, MIPS_CATEGORY_NONE, MIPS_FORMAT_C2, MIPS_V1 }; // 00100
    mips_opcodes_c2[0x02] = (MIPSOpcode){MIPS_INSTR_CFC2, MIPS_CATEGORY_NONE, MIPS_FORMAT_C2, MIPS_V1 }; // 00010
    mips_opcodes_c2[0x06] = (MIPSOpcode){MIPS_INSTR_CTC2, MIPS_CATEGORY_NONE, MIPS_FORMAT_C2, MIPS_V1 }; // 00110

    mips_opcodes_cls[0x31] = (MIPSOpcode){MIPS_INSTR_LWC1, MIPS_CATEGORY_NONE, MIPS_FORMAT_CLS, MIPS_V1 }; // 110001
    mips_opcodes_cls[0x39] = (MIPSOpcode){MIPS_INSTR_SWC1, MIPS_CATEGORY_NONE, MIPS_FORMAT_CLS, MIPS_V1 }; // 111001
    mips_opcodes_cls[0x32] = (MIPSOpcode){MIPS_INSTR_LWC2, MIPS_CATEGORY_NONE, MIPS_FORMAT_CLS, MIPS_V1 }; // 110010
    mips_opcodes_cls[0x3A] = (MIPSOpcode){MIPS_INSTR_LWC2, MIPS_CATEGORY_NONE, MIPS_FORMAT_CLS, MIPS_V1 }; // 111010
    // clang-format on
}

static int _mips_check_format(const MIPSInstruction* mi) {
    if(!mi->r.op) {
        switch(mi->b.funct) {
            case 0x0C: // 001100
            case 0x0D: // 001101
                return MIPS_FORMAT_B;

            case 0x30: // 110000
            case 0x34: // 110100
                return MIPS_FORMAT_C;

            default: break;
        }

        return MIPS_FORMAT_R;
    }

    switch(mi->unk.op) {
        case 0x10:
            return MIPS_FORMAT_C0; // 010000
        // case 0x11: return MIPS_FORMAT_C1; // 010001
        case 0x12: return MIPS_FORMAT_C2; // 010010

        case 0x31: // 110001
        case 0x39: // 111001
        case 0x32: // 110010
        case 0x3A: // 111010
            return MIPS_FORMAT_CLS;

        default: break;
    }

    if(((mi->i_u.op >= 0x04) && (mi->i_u.op <= 0x2e)) || (mi->i_u.op == 0x01))
        return MIPS_FORMAT_I;
    if((mi->j.op == 0x02) || (mi->j.op == 0x03)) return MIPS_FORMAT_J;

    return MIPS_FORMAT_NONE;
}

static bool _mips_check_encoding(MIPSDecodedInstruction* dec) {
    int f = _mips_check_format(&dec->instr);

    switch(f) {
        case MIPS_FORMAT_R:
            dec->opcode = &mips_opcodes_r[dec->instr.r.funct];
            break;

        case MIPS_FORMAT_C:
            dec->opcode = &mips_opcodes_c[dec->instr.c.funct];
            break;

        case MIPS_FORMAT_I:
            dec->opcode = &mips_opcodes_i[dec->instr.i_u.op];
            break;

        case MIPS_FORMAT_J:
            dec->opcode = &mips_opcodes_j[dec->instr.j.op];
            break;

        case MIPS_FORMAT_B:
            dec->opcode = &mips_opcodes_b[dec->instr.b.funct];
            break;

        case MIPS_FORMAT_C0:
            dec->opcode = &mips_opcodes_c0[dec->instr.c0sel.code];
            break;

        case MIPS_FORMAT_C2:
            dec->opcode = &mips_opcodes_c2[dec->instr.c2impl.code];
            break;

        case MIPS_FORMAT_CLS:
            dec->opcode = &mips_opcodes_cls[dec->instr.cls.op];
            break;

        default:
            dec->instr = (MIPSInstruction){0};
            dec->opcode = NULL;
            return false;
    }

    return f != MIPS_FORMAT_NONE;
}

RDAddress mips_calc_addr16(RDAddress pc, i16 imm) {
    return pc + sizeof(MIPSInstruction) + ((i32)imm * sizeof(MIPSInstruction));
}

RDAddress mips_calc_addr26(RDAddress pc, u32 imm) {
    u32 pcupper = (pc + sizeof(MIPSInstruction)) & 0xF0000000;
    return pcupper | imm << 2;
}

void mips_decode_r(const MIPSDecodedInstruction* dec, RDInstruction* instr) {
    switch(instr->id) {
        case MIPS_INSTR_JALR:
        case MIPS_INSTR_JR:
            instr->operands[0].kind = RD_OP_REG;
            instr->operands[0].reg = dec->instr.r.rs;
            break;

        default: {
            instr->operands[0].kind = RD_OP_REG;
            instr->operands[0].reg = dec->instr.r.rd;
            instr->operands[1].kind = RD_OP_REG;
            instr->operands[1].reg = dec->instr.r.rs;
            instr->operands[2].kind = RD_OP_REG;
            instr->operands[2].reg = dec->instr.r.rt;
            break;
        }
    }
}

void mips_decode_i(const MIPSDecodedInstruction* dec, RDInstruction* instr) {
    switch(dec->opcode->id) {
        case MIPS_INSTR_LB:
        case MIPS_INSTR_LBU:
        case MIPS_INSTR_LH:
        case MIPS_INSTR_LHU:
        case MIPS_INSTR_LW:
        case MIPS_INSTR_LWL:
        case MIPS_INSTR_LWR:
        case MIPS_INSTR_SB:
        case MIPS_INSTR_SH:
        case MIPS_INSTR_SWL:
        case MIPS_INSTR_SWR:
            instr->operands[0].kind = RD_OP_REG;
            instr->operands[0].reg = dec->instr.i_u.rt;

            instr->operands[1].kind = RD_OP_DISPL;
            instr->operands[1].displ.base = dec->instr.i_u.rs;
            instr->operands[1].displ.displ = dec->instr.i_u.imm;
            return;

        case MIPS_INSTR_LUI:
            instr->operands[0].kind = RD_OP_REG;
            instr->operands[0].reg = dec->instr.i_u.rt;
            instr->operands[1].kind = RD_OP_IMM;
            instr->operands[1].imm = dec->instr.i_u.imm;
            return;

        default: break;
    }

    instr->operands[0].kind = RD_OP_REG;
    instr->operands[0].reg = dec->instr.i_u.rt;

    instr->operands[1].kind = RD_OP_REG;
    instr->operands[1].reg = dec->instr.i_u.rs;

    if(dec->opcode->category == MIPS_CATEGORY_JUMP_COND) {
        instr->operands[2].kind = RD_OP_ADDR;
        instr->operands[2].addr =
            mips_calc_addr16(instr->address, dec->instr.i_s.imm);
    }
    else {
        instr->operands[2].kind = RD_OP_IMM;
        instr->operands[2].imm = dec->instr.i_u.imm;
    }
}

void mips_decode_j(const MIPSDecodedInstruction* dec, RDInstruction* instr) {
    instr->operands[0].kind = RD_OP_ADDR;
    instr->operands[0].addr =
        mips_calc_addr26(instr->address, (u32)dec->instr.j.target);
}

void mips_decode_b(const MIPSDecodedInstruction* dec, RDInstruction* instr) {
    if(dec->opcode->id == MIPS_INSTR_BREAK) instr->flow = RD_IF_STOP;
}

bool mips_has_delay_slot(usize id) {
    switch(id) {
        case MIPS_MACRO_B:
        case MIPS_MACRO_BEQZ:
        case MIPS_MACRO_BNEZ:
        case MIPS_INSTR_BEQ:
        case MIPS_INSTR_BGEZ:
        case MIPS_INSTR_BGTZ:
        case MIPS_INSTR_BLEZ:
        case MIPS_INSTR_BNE:
        case MIPS_INSTR_J:
        case MIPS_INSTR_JAL:
        case MIPS_INSTR_JALR:
        case MIPS_INSTR_JR: return true;

        default: break;
    }

    return false;
}

bool mips_decode_one_be(const RDContext* ctx, RDAddress address,
                        MIPSDecodedInstruction* dec) {
    if(!rd_read_be32(ctx, address, &dec->instr.word)) return false;

    dec->length = sizeof(MIPSInstruction);
    return _mips_check_encoding(dec);
}

bool mips_decode_one_le(const RDContext* ctx, RDAddress address,
                        MIPSDecodedInstruction* dec) {
    if(!rd_read_le32(ctx, address, &dec->instr.word)) return false;

    dec->length = sizeof(MIPSInstruction);
    return _mips_check_encoding(dec);
}
