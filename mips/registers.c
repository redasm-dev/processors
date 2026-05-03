#include "registers.h"
#include "decoder/registers.h"

bool mips_get_regval(RDContext* ctx, RDAddress addr, RDReg reg,
                     RDRegValue* val) {
    if(reg == MIPS_REG_ZERO) {
        *val = 0;
        return true; // always valid, never goes to DB
    }

    return rd_get_regval_id(ctx, addr, reg, val);
}

void mips_set_regval(RDContext* ctx, RDAddress addr, RDReg reg,
                     RDRegValue val) {
    if(reg == MIPS_REG_ZERO) return; // discard, mirrors hardware
    rd_auto_regval_id(ctx, addr, reg, val);
}
