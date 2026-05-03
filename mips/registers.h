#pragma once

#include <redasm/redasm.h>

bool mips_get_regval(RDContext* ctx, RDAddress addr, RDReg reg,
                     RDRegValue* val);
void mips_set_regval(RDContext* ctx, RDAddress addr, RDReg reg, RDRegValue val);
