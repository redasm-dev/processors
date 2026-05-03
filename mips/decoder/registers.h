#pragma once

#include <redasm/redasm.h>

typedef enum {
    MIPS_REG_ZERO = 0,
    MIPS_REG_AT,
    MIPS_REG_V0,
    MIPS_REG_V1,
    MIPS_REG_A0,
    MIPS_REG_A1,
    MIPS_REG_A2,
    MIPS_REG_A3,
    MIPS_REG_T0,
    MIPS_REG_T1,
    MIPS_REG_T2,
    MIPS_REG_T3,
    MIPS_REG_T4,
    MIPS_REG_T5,
    MIPS_REG_T6,
    MIPS_REG_T7,
    MIPS_REG_S0,
    MIPS_REG_S1,
    MIPS_REG_S2,
    MIPS_REG_S3,
    MIPS_REG_S4,
    MIPS_REG_S5,
    MIPS_REG_S6,
    MIPS_REG_S7,
    MIPS_REG_T8,
    MIPS_REG_T9,
    MIPS_REG_K0,
    MIPS_REG_K1,
    MIPS_REG_GP,
    MIPS_REG_SP,
    MIPS_REG_FP,
    MIPS_REG_RA,

    MIPS_REG_MAX,
} MIPSRegisterId;

typedef enum {
    MIPS_REG_COP0_INDEX = 0,
    MIPS_REG_COP0_RANDOM = 1,
    MIPS_REG_COP0_ENTRY_LO0 = 2,
    MIPS_REG_COP0_ENTRY_LO1 = 3,
    MIPS_REG_COP0_CONTEXT = 4,
    MIPS_REG_COP0_PAGE_MASK = 5,
    MIPS_REG_COP0_WIRED = 6,
    MIPS_REG_COP0_RESERVED = 7,
    MIPS_REG_COP0_BAD_V_ADDR = 8,
    MIPS_REG_COP0_COUNT = 9,
    MIPS_REG_COP0_ENTRY_HI = 10,
    MIPS_REG_COP0_COMPARE = 11,
    MIPS_REG_COP0_STATUS = 12,
    MIPS_REG_COP0_CAUSE = 13,
    MIPS_REG_COP0_EPC = 14,
    MIPS_REG_COP0_PR_ID = 15,
    MIPS_REG_COP0_CONFIG = 16,
    MIPS_REG_COP0_LL_ADDR = 17,
    MIPS_REG_COP0_WATCH_LO = 18,
    MIPS_REG_COP0_WATCH_HI = 19,
    MIPS_REG_COP0_X_CONTEXT = 20,

    MIPS_REG_COP0_CACHE_ERR = 27,
    MIPS_REG_COP0_TAG_LO = 28,
    MIPS_REG_CO_P0_TAG_HI = 29,
    MIPS_REG_COP0_ERROR_EPC = 30,

    MIPS_REG_COP0_MAX = 32,
} MIPSCOP0RegisterId;

const char* mips_get_register(int r);
const char* mips_get_cop0_register(int r);
const char* mips_get_copn_register(int r);
