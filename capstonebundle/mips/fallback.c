#include "fallback.h"

// hardware register number -> capstone id (capstone's enum is alphabetical)
static const mips_reg MIPS_GPR[32] = {
    MIPS_REG_ZERO, MIPS_REG_AT, MIPS_REG_V0, MIPS_REG_V1, MIPS_REG_A0,
    MIPS_REG_A1,   MIPS_REG_A2, MIPS_REG_A3, MIPS_REG_T0, MIPS_REG_T1,
    MIPS_REG_T2,   MIPS_REG_T3, MIPS_REG_T4, MIPS_REG_T5, MIPS_REG_T6,
    MIPS_REG_T7,   MIPS_REG_S0, MIPS_REG_S1, MIPS_REG_S2, MIPS_REG_S3,
    MIPS_REG_S4,   MIPS_REG_S5, MIPS_REG_S6, MIPS_REG_S7, MIPS_REG_T8,
    MIPS_REG_T9,   MIPS_REG_K0, MIPS_REG_K1, MIPS_REG_GP, MIPS_REG_SP,
    MIPS_REG_FP,   MIPS_REG_RA,
};

static const mips_reg MIPS64_GPR[32] = {
    MIPS_REG_ZERO_64, MIPS_REG_AT_64, MIPS_REG_V0_64, MIPS_REG_V1_64,
    MIPS_REG_A0_64,   MIPS_REG_A1_64, MIPS_REG_A2_64, MIPS_REG_A3_64,
    MIPS_REG_T0_64,   MIPS_REG_T1_64, MIPS_REG_T2_64, MIPS_REG_T3_64,
    MIPS_REG_T4_64,   MIPS_REG_T5_64, MIPS_REG_T6_64, MIPS_REG_T7_64,
    MIPS_REG_S0_64,   MIPS_REG_S1_64, MIPS_REG_S2_64, MIPS_REG_S3_64,
    MIPS_REG_S4_64,   MIPS_REG_S5_64, MIPS_REG_S6_64, MIPS_REG_S7_64,
    MIPS_REG_T8_64,   MIPS_REG_T9_64, MIPS_REG_K0_64, MIPS_REG_K1_64,
    MIPS_REG_GP_64,   MIPS_REG_SP_64, MIPS_REG_FP_64, MIPS_REG_RA_64,
};

static const mips_reg MIPS_COP2[32] = {
    MIPS_REG_COP20,  MIPS_REG_COP21,  MIPS_REG_COP22,  MIPS_REG_COP23,
    MIPS_REG_COP24,  MIPS_REG_COP25,  MIPS_REG_COP26,  MIPS_REG_COP27,
    MIPS_REG_COP28,  MIPS_REG_COP29,  MIPS_REG_COP210, MIPS_REG_COP211,
    MIPS_REG_COP212, MIPS_REG_COP213, MIPS_REG_COP214, MIPS_REG_COP215,
    MIPS_REG_COP216, MIPS_REG_COP217, MIPS_REG_COP218, MIPS_REG_COP219,
    MIPS_REG_COP220, MIPS_REG_COP221, MIPS_REG_COP222, MIPS_REG_COP223,
    MIPS_REG_COP224, MIPS_REG_COP225, MIPS_REG_COP226, MIPS_REG_COP227,
    MIPS_REG_COP228, MIPS_REG_COP229, MIPS_REG_COP230, MIPS_REG_COP231,
};

static mips_reg _mips_gpr(const Capstone* cs, u32 n) {
    return (cs->data->mode & CS_MODE_MIPS64) ? MIPS64_GPR[n] : MIPS_GPR[n];
}

static u32 _mips_word(const char* data, const Capstone* cs) {
    const u8* b = (const u8*)data;

    if(cs->data->mode & CS_MODE_BIG_ENDIAN)
        return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3];

    return ((u32)b[3] << 24) | ((u32)b[2] << 16) | ((u32)b[1] << 8) | b[0];
}

// standard cfc2/ctc2: capstone maps only the microMIPS opcodes
// https://github.com/capstone-engine/capstone/issues/754
//
//   cfc2 rt, rd:  010010 00010 rt rd 00000000000
//   ctc2 rt, rd:  010010 00110 rt rd 00000000000
static bool _mips_cop2_ctl(const Capstone* cs, u32 w, RDInstruction* instr) {
    if((w >> 26) != 0x12 || (w & 0x7FF)) return false;

    u32 rs = (w >> 21) & 0x1F;
    if(rs != 0x02 && rs != 0x06) return false;

    instr->id = rs == 0x02 ? MIPS_INS_CFC2 : MIPS_INS_CTC2;

    RDOperand* op = &instr->operands[0];
    op->kind = RD_OP_REG;
    op->reg = _mips_gpr(cs, (w >> 16) & 0x1F); // rt

    op = &instr->operands[1];
    op->kind = RD_OP_IMM;
    op->kind = RD_OP_REG;
    op->reg = MIPS_COP2[(w >> 11) & 0x1F]; // rd: control register
    return true;
}

// cop2 imm25: coprocessor operation, no GPR access
//
//   010010 1 cofun(25)
static bool _mips_cop2_op(u32 w, RDInstruction* instr) {
    if((w >> 26) != 0x12 || !(w & (1U << 25))) return false;

    instr->id = MIPS_X_INS_COP2;

    RDOperand* op = &instr->operands[0];
    op->kind = RD_OP_IMM;
    op->imm = w & 0x1FFFFFF;
    return true;
}

static void _mips_fallback_set_mnemonic(RDInstruction* instr) {
    switch(instr->id) {
        case MIPS_INS_CFC2: rd_instr_set_mnemonic(instr, "cfc2"); break;
        case MIPS_INS_CTC2: rd_instr_set_mnemonic(instr, "ctc2"); break;
        case MIPS_X_INS_COP2: rd_instr_set_mnemonic(instr, "cop2"); break;
        default: break;
    }
}

bool mips_fallback_decode(const char* data, const Capstone* cs,
                          RDInstruction* instr) {
    u32 w = _mips_word(data, cs);
    if(!_mips_cop2_ctl(cs, w, instr) && !_mips_cop2_op(w, instr)) return false;

    _mips_fallback_set_mnemonic(instr);
    instr->length = sizeof(u32);
    return true;
}
