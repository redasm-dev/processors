#pragma once

#include "capstone.h"
#include <redasm/redasm.h>

// instructions capstone has no id for
enum {
    MIPS_X_INS_COP2 = MIPS_INS_ALIAS_END + 1,
};

// decodes what capstone refuses (called only when capstone_plugin_decode fails)
bool mips_fallback_decode(const char* data, const Capstone* cs,
                          RDInstruction* instr);
