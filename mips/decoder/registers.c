#include "registers.h"

static const char* const GPR_NAMES[MIPS_REG_MAX] = {
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0",   "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0",   "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8",   "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
};

static const char* const COP0R_NAMES[MIPS_REG_COP0_MAX] = {
    "$Index",    "$Random",   "$EntryLo0", "$EntryLo1", "$Context", "$PageMask",
    "$Wired",    "$Reserved", "$BadVAddr", "$Count",    "$EntryHi", "$Compare",
    "$Status",   "$Cause",    "$EPC",      "PRId",      "$Config",  "$LLAddr",
    "$WatchLo",  "$WatchHi",  "$XContext", "$21",       "$22",      "$23",
    "$24",       "$25",       "$26",       "$CacheErr", "$TagLo",   "$TagHi",
    "$ErrorEPC", "$31",
};

const char* mips_get_register(int r) {
    return r < MIPS_REG_MAX ? GPR_NAMES[r] : "???";
}

const char* mips_get_cop0_register(int r) {
    return r < MIPS_REG_COP0_MAX ? COP0R_NAMES[r] : "???";
}

const char* mips_get_copn_register(int r) { return "copn_reg"; }
