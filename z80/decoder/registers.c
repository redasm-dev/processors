#include "registers.h"
#include "decoder.h"
#include <string.h>

static Z80RegId _z80_reg_canonical(Z80RegId id) {
    switch(id) {
        case Z80_REG_B:
        case Z80_REG_C: return Z80_REG_BC;

        case Z80_REG_D:
        case Z80_REG_E: return Z80_REG_DE;

        case Z80_REG_H:
        case Z80_REG_L: return Z80_REG_HL;

        case Z80_REG_IXH:
        case Z80_REG_IXL: return Z80_REG_IX;

        case Z80_REG_IYH:
        case Z80_REG_IYL:
            return Z80_REG_IY;

            // A, HL, BC, DE, SP, IX, IY, I, R, shadows: already canonical
        default: return id;
    }
}

static Z80RegId _z80_get_reg_id(const char* name) {
    if(!name) return Z80_REG_INVALID;

    if(!strcmp(name, "a")) return Z80_REG_A;
    if(!strcmp(name, "b")) return Z80_REG_B;
    if(!strcmp(name, "c")) return Z80_REG_C;
    if(!strcmp(name, "d")) return Z80_REG_D;
    if(!strcmp(name, "e")) return Z80_REG_E;
    if(!strcmp(name, "h")) return Z80_REG_H;
    if(!strcmp(name, "l")) return Z80_REG_L;

    if(!strcmp(name, "bc")) return Z80_REG_BC;
    if(!strcmp(name, "de")) return Z80_REG_DE;
    if(!strcmp(name, "hl")) return Z80_REG_HL;
    if(!strcmp(name, "sp")) return Z80_REG_SP;
    if(!strcmp(name, "af")) return Z80_REG_AF;

    if(!strcmp(name, "i")) return Z80_REG_I;
    if(!strcmp(name, "r")) return Z80_REG_R;

    if(!strcmp(name, "ix")) return Z80_REG_IX;
    if(!strcmp(name, "ixh")) return Z80_REG_IXH;
    if(!strcmp(name, "ixl")) return Z80_REG_IXL;
    if(!strcmp(name, "iy")) return Z80_REG_IY;
    if(!strcmp(name, "iyh")) return Z80_REG_IYH;
    if(!strcmp(name, "iyl")) return Z80_REG_IYL;

    if(!strcmp(name, "bc'")) return Z80_REG_SHD_BC;
    if(!strcmp(name, "de'")) return Z80_REG_SHD_DE;
    if(!strcmp(name, "hl'")) return Z80_REG_SHD_HL;
    if(!strcmp(name, "af'")) return Z80_REG_SHD_AF;

    return Z80_REG_INVALID;
}

static u64 _z80_reg_width_mask(Z80RegId id) {
    switch(id) {
        case Z80_REG_A:
        case Z80_REG_B:
        case Z80_REG_C:
        case Z80_REG_D:
        case Z80_REG_E:
        case Z80_REG_H:
        case Z80_REG_L:
        case Z80_REG_IXH:
        case Z80_REG_IXL:
        case Z80_REG_IYH:
        case Z80_REG_IYL:
        case Z80_REG_I:
        case Z80_REG_R: return 0xFF; // 8-bit

        default: return 0xFFFF; // BC, DE, HL, SP, AF, IX, IY, shadows: 16-bit
    }
}

static const char* _z80_get_reg_name(RDReg reg) {
    switch(reg) {
        case Z80_REG_I: return "i";
        case Z80_REG_R: return "r";

        case Z80_REG_A: return "a";
        case Z80_REG_B: return "b";
        case Z80_REG_C: return "c";
        case Z80_REG_D: return "d";
        case Z80_REG_E: return "e";
        case Z80_REG_H: return "h";
        case Z80_REG_L: return "l";

        case Z80_REG_BC: return "bc";
        case Z80_REG_DE: return "de";
        case Z80_REG_HL: return "hl";
        case Z80_REG_SP: return "sp";
        case Z80_REG_AF: return "af";

        case Z80_REG_IX: return "ix";
        case Z80_REG_IXH: return "ixh";
        case Z80_REG_IXL: return "ixl";
        case Z80_REG_IY: return "iy";
        case Z80_REG_IYH: return "iyh";
        case Z80_REG_IYL: return "iyl";

        case Z80_REG_SHD_BC: return "bc'";
        case Z80_REG_SHD_DE: return "de'";
        case Z80_REG_SHD_HL: return "hl'";
        case Z80_REG_SHD_AF: return "af'";

        default: break;
    }

    return NULL;
}

static bool _z80_get_reg_mask(const char* name, RDRegMask* m) {
    Z80RegId id = _z80_get_reg_id(name);
    if(id == Z80_REG_INVALID) return false;

    switch(id) {
        case Z80_REG_H:
        case Z80_REG_B:
        case Z80_REG_D:
        case Z80_REG_IXH:
        case Z80_REG_IYH:
            m->mask = 0xFF00;
            m->shift = 8;
            break;

        case Z80_REG_L:
        case Z80_REG_C:
        case Z80_REG_E:
        case Z80_REG_IXL:
        case Z80_REG_IYL: m->mask = 0x00FF; break;

        default: m->mask = RD_REGMASK_FULL; break;
    }

    return true;
}

const char* z80_cc_name(Z80Condition cc) {
    switch(cc) {
        case Z80_CC_NZ: return "nz";
        case Z80_CC_Z: return "z";
        case Z80_CC_NC: return "nc";
        case Z80_CC_C: return "c";
        case Z80_CC_PO: return "po";
        case Z80_CC_PE: return "pe";
        case Z80_CC_P: return "p";
        case Z80_CC_M: return "m";
        default: break;
    }

    return "???";
}

bool z80_query_reg(RDQueryReg* q, RDProcessor* p) {
    RD_UNUSED(p);

    if(q->kind == RD_QUERY_REG_BY_ID) {
        q->name = _z80_get_reg_name(q->id);
        if(!q->name) return false;
    }
    else if(q->kind == RD_QUERY_REG_BY_NAME) {
        q->id = _z80_get_reg_id(q->name);
        if(q->id == Z80_REG_INVALID) return false;
    }
    else
        return false;

    if(q->want & RD_QUERY_REG_WANT_MASK) {
        if(!_z80_get_reg_mask(q->name, &q->mask)) return false;
    }

    if(q->want & RD_QUERY_REG_WANT_CANONICAL) {
        q->canonical_name = _z80_get_reg_name(_z80_reg_canonical(q->id));
        if(!q->canonical_name) return false;
    }

    return true;
}

void z80_track_regs(RDContext* ctx, const RDInstruction* instr) {
    const RDOperand* dst = &instr->operands[0];
    const RDOperand* src = &instr->operands[1];

    switch(instr->id) {
        case Z80_INSTR_LD: {
            if(dst->kind != RD_OP_REG) break;

            switch(src->kind) {
                case RD_OP_IMM:
                    rd_set_regval_id(ctx, dst->reg, src->imm);
                    break;

                case RD_OP_REG: {
                    RDRegValue v;
                    if(rd_get_regval_id(ctx, src->reg, &v))
                        rd_set_regval_id(ctx, dst->reg, v);
                    else
                        rd_del_regval_id(ctx, dst->reg);
                    break;
                }

                // memory-sourced (LD r,(HL) / LD rp,(nn) / etc.)
                // unknown statically
                default: rd_del_regval_id(ctx, dst->reg); break;
            }
            break;
        }

        case Z80_INSTR_INC:
        case Z80_INSTR_DEC: {
            if(dst->kind != RD_OP_REG) break;
            RDRegValue v;
            if(rd_get_regval_id(ctx, dst->reg, &v)) {
                v += (instr->id == Z80_INSTR_INC) ? 1 : -1;
                rd_set_regval_id(ctx, dst->reg,
                                 v & _z80_reg_width_mask((Z80RegId)dst->reg));
            }
            break;
        }

            // ADD HL,rp only, ADC/SBC need carry-in, not tracked
        case Z80_INSTR_ADD: {
            if(dst->kind != RD_OP_REG || src->kind != RD_OP_REG) break;

            RDRegValue a, b;
            if(rd_get_regval_id(ctx, dst->reg, &a) &&
               rd_get_regval_id(ctx, src->reg, &b))
                rd_set_regval_id(ctx, dst->reg, (a + b) & 0xFFFF);
            else
                rd_del_regval_id(ctx, dst->reg);
            break;
        }

        // ADC/SBC HL,rp: carry flag isn't tracked
        case Z80_INSTR_ADC:
        case Z80_INSTR_SBC:
        // POP value comes from stack, (same conservative call as x86_track_pop)
        case Z80_INSTR_POP:
        // EX DE,HL / EX (SP),HL swap/stack-load,
        // invalidate rather than attempt to swap tracked state
        case Z80_INSTR_EX: {
            if(dst->kind == RD_OP_REG) rd_del_regval_id(ctx, dst->reg);
            if(src->kind == RD_OP_REG) rd_del_regval_id(ctx, src->reg);
            break;
        }

        default: break;
    }
}

void z80_track_derefs(RDContext* ctx, const RDInstruction* instr) {
    for(int i = 0; i < 2; i++) {
        const RDOperand* op = &instr->operands[i];
        if(op->kind != Z80_USEROP_IND_REG) continue;

        RDRegValue v;
        if(rd_get_regval_id(ctx, op->reg, &v)) {
            rd_add_xref(ctx, instr->address, v,
                        (i == 0) ? RD_DR_WRITE : RD_DR_READ);
        }
    }
}
