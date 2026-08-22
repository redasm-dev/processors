#pragma once

#include <redasm/redasm.h>

// clang-format off
typedef enum {
    Z80_REG_INVALID = 0,
    Z80_REG_A, Z80_REG_B, Z80_REG_C, Z80_REG_D, Z80_REG_E, Z80_REG_H, Z80_REG_L,
    Z80_REG_BC, Z80_REG_DE, Z80_REG_HL, Z80_REG_SP, Z80_REG_AF,
    Z80_REG_I, Z80_REG_R,

    Z80_REG_IX, Z80_REG_IXH, Z80_REG_IXL,
    Z80_REG_IY, Z80_REG_IYH, Z80_REG_IYL,

    // shadow registers
    Z80_REG_SHD_BC, Z80_REG_SHD_DE, Z80_REG_SHD_HL, Z80_REG_SHD_AF,
} Z80RegId;
// clang-format on

typedef enum {
    Z80_CC_NZ = 0,
    Z80_CC_Z,
    Z80_CC_NC,
    Z80_CC_C,
    Z80_CC_PO,
    Z80_CC_PE,
    Z80_CC_P,
    Z80_CC_M,
} Z80Condition;

const char* z80_cc_name(Z80Condition cc);
bool z80_query_reg(RDQueryReg* q, RDProcessor* p);
void z80_track_regs(RDContext* ctx, const RDInstruction* instr);
void z80_track_derefs(RDContext* ctx, const RDInstruction* instr);
