#include "registers.h"
#include <Zydis/Zydis.h>
#include <string.h>

static RDReg _x86_get_register_id(const char* name) {
    for(ZydisRegister r = ZYDIS_REGISTER_NONE + 1; r < ZYDIS_REGISTER_MAX_VALUE;
        r++) {
        const char* rname = ZydisRegisterGetString(r);
        if(rname && !strcmp(rname, name)) return (RDReg)r;
    }

    return RD_REGID_INVALID;
}

static RDReg _x86_canonical_reg(RDReg r) {
    switch(r) {
        case ZYDIS_REGISTER_AL:
        case ZYDIS_REGISTER_AH:
        case ZYDIS_REGISTER_AX:
        case ZYDIS_REGISTER_EAX: return ZYDIS_REGISTER_RAX;

        case ZYDIS_REGISTER_BL:
        case ZYDIS_REGISTER_BH:
        case ZYDIS_REGISTER_BX:
        case ZYDIS_REGISTER_EBX: return ZYDIS_REGISTER_RBX;

        case ZYDIS_REGISTER_CL:
        case ZYDIS_REGISTER_CH:
        case ZYDIS_REGISTER_CX:
        case ZYDIS_REGISTER_ECX: return ZYDIS_REGISTER_RCX;

        case ZYDIS_REGISTER_DL:
        case ZYDIS_REGISTER_DH:
        case ZYDIS_REGISTER_DX:
        case ZYDIS_REGISTER_EDX: return ZYDIS_REGISTER_RDX;

        case ZYDIS_REGISTER_SI:
        case ZYDIS_REGISTER_ESI: return ZYDIS_REGISTER_RSI;

        case ZYDIS_REGISTER_DI:
        case ZYDIS_REGISTER_EDI: return ZYDIS_REGISTER_RDI;

        case ZYDIS_REGISTER_BP:
        case ZYDIS_REGISTER_EBP: return ZYDIS_REGISTER_RBP;

        case ZYDIS_REGISTER_SP:
        case ZYDIS_REGISTER_ESP: return ZYDIS_REGISTER_RSP;

        default: break;
    }

    return r;
}

// register I/O is relative to 64 bit registers
static bool _x86_get_reg_mask(RDReg reg_id, RDRegMask* m) {
    if(reg_id == RD_REGID_INVALID) return false;

    switch(reg_id) {
        case ZYDIS_REGISTER_AL:
        case ZYDIS_REGISTER_BL:
        case ZYDIS_REGISTER_CL:
        case ZYDIS_REGISTER_DL: m->mask = 0xFF; break;

        case ZYDIS_REGISTER_AH:
        case ZYDIS_REGISTER_BH:
        case ZYDIS_REGISTER_CH:
        case ZYDIS_REGISTER_DH:
            m->mask = 0xFF00;
            m->shift = 8;
            break;

        case ZYDIS_REGISTER_AX:
        case ZYDIS_REGISTER_BX:
        case ZYDIS_REGISTER_CX:
        case ZYDIS_REGISTER_DX:
        case ZYDIS_REGISTER_SI:
        case ZYDIS_REGISTER_DI:
        case ZYDIS_REGISTER_BP:
        case ZYDIS_REGISTER_SP: m->mask = 0xFFFF; break;

        case ZYDIS_REGISTER_EAX:
        case ZYDIS_REGISTER_EBX:
        case ZYDIS_REGISTER_ECX:
        case ZYDIS_REGISTER_EDX:
        case ZYDIS_REGISTER_ESI:
        case ZYDIS_REGISTER_EDI:
        case ZYDIS_REGISTER_EBP:
        case ZYDIS_REGISTER_ESP: m->mask = 0xFFFFFFFF; break;

        default: m->mask = RD_REGMASK_FULL; break;
    }

    return true;
}

bool x86_is_segment_reg(RDReg r) {
    switch(r) {
        case ZYDIS_REGISTER_ES:
        case ZYDIS_REGISTER_CS:
        case ZYDIS_REGISTER_SS:
        case ZYDIS_REGISTER_DS:
        case ZYDIS_REGISTER_FS:
        case ZYDIS_REGISTER_GS: return true;

        default: break;
    }

    return false;
}

void x86_set_regval(RDContext* ctx, RDAddress address, RDReg id, RDRegValue v) {
    if(x86_is_segment_reg(id))
        rd_auto_sregval_id(ctx, address, id, v);
    else
        rd_set_regval_id(ctx, id, v);
}

bool x86_get_regval(RDContext* ctx, RDAddress address, RDReg id,
                    RDRegValue* v) {
    if(x86_is_segment_reg(id)) return rd_get_sregval_id(ctx, address, id, v);
    return rd_get_regval_id(ctx, id, v);
}

void x86_del_regval(RDContext* ctx, RDAddress address, RDReg id) {
    if(x86_is_segment_reg(id))
        rd_del_auto_sregval_id(ctx, address, id);
    else
        rd_del_regval_id(ctx, id);
}

RDAddress x86_get_ip_value(const RDInstruction* instr) {
    return instr->address + instr->length;
}

void x86_track_math(RDContext* ctx, const RDInstruction* instr) {
    if(instr->operands[0].kind != RD_OP_REG) return;

    const RDOperand* dst = &instr->operands[0];
    RDAddress next = x86_get_ip_value(instr);

    RDRegValue lhs, rhs, result;
    bool lhs_known = x86_get_regval(ctx, instr->address, dst->reg, &lhs);

    switch(instr->id) {
        case ZYDIS_MNEMONIC_INC: {
            if(lhs_known)
                x86_set_regval(ctx, next, dst->reg, lhs + 1);
            else
                x86_del_regval(ctx, next, dst->reg);
            return;
        }

        case ZYDIS_MNEMONIC_DEC: {
            if(lhs_known)
                x86_set_regval(ctx, next, dst->reg, lhs - 1);
            else
                x86_del_regval(ctx, next, dst->reg);
            return;
        }

        case ZYDIS_MNEMONIC_NOT: {
            if(lhs_known)
                x86_set_regval(ctx, next, dst->reg, ~lhs);
            else
                x86_del_regval(ctx, next, dst->reg);
            return;
        }

        case ZYDIS_MNEMONIC_NEG: {
            if(lhs_known)
                x86_set_regval(ctx, next, dst->reg, (RDRegValue)(-(i64)lhs));
            else
                x86_del_regval(ctx, next, dst->reg);
            return;
        }

        default: break;
    }

    // binary operations, need src too
    if(instr->operands[1].kind == RD_OP_NULL) return;

    const RDOperand* src = &instr->operands[1];

    bool rhs_known = false;
    if(src->kind == RD_OP_IMM) {
        rhs = src->imm;
        rhs_known = true;
    }
    else if(src->kind == RD_OP_REG)
        rhs_known = x86_get_regval(ctx, instr->address, src->reg, &rhs);

    if(!lhs_known || !rhs_known) {
        x86_del_regval(ctx, next, dst->reg);
        return;
    }

    switch(instr->id) {
        case ZYDIS_MNEMONIC_ADD: result = lhs + rhs; break;
        case ZYDIS_MNEMONIC_SUB: result = lhs - rhs; break;
        case ZYDIS_MNEMONIC_AND: result = lhs & rhs; break;
        case ZYDIS_MNEMONIC_OR: result = lhs | rhs; break;

        case ZYDIS_MNEMONIC_XOR: {
            // XOR reg, reg = always zero
            if(src->kind == RD_OP_REG && src->reg == dst->reg)
                result = 0;
            else
                result = lhs ^ rhs;
            break;
        }

        case ZYDIS_MNEMONIC_SHL: result = lhs << (rhs & 0x3F); break;
        case ZYDIS_MNEMONIC_SHR: result = lhs >> (rhs & 0x3F); break;

        case ZYDIS_MNEMONIC_SAR: {
            result = (RDRegValue)((i64)lhs >> (rhs & 0x3F));
            break;
        }

        default: x86_del_regval(ctx, next, dst->reg); return;
    }

    x86_set_regval(ctx, next, dst->reg, result);
}

void x86_track_mov(RDContext* ctx, const RDInstruction* instr) {
    if(instr->operands[0].kind != RD_OP_REG) return;

    const RDOperand* dst = &instr->operands[0];
    const RDOperand* src = &instr->operands[1];
    RDAddress next = x86_get_ip_value(instr);

    switch(src->kind) {
        case RD_OP_IMM: x86_set_regval(ctx, next, dst->reg, src->imm); break;

        case RD_OP_ADDR: x86_set_regval(ctx, next, dst->reg, src->addr); break;

        case RD_OP_REG: {
            u64 v;

            if(x86_get_regval(ctx, instr->address, src->reg, &v))
                x86_set_regval(ctx, next, dst->reg, v);
            else
                x86_del_regval(ctx, next, dst->reg);

            break;
        }

        default: { // memory source: invalidate if 'dst' is a segment reg
            if(dst->kind == RD_OP_REG && x86_is_segment_reg(dst->reg))
                x86_del_regval(ctx, next, dst->reg);

            break;
        }
    }
}

bool x86_track_pop(RDContext* ctx, const RDInstruction* instr) {
    // TODO: stack tracking needed to resolve POP sreg values
    // statically. For now, always invalidate, correct but conservative.
    const RDOperand* dst = &instr->operands[0];
    if(dst->kind != RD_OP_REG) return false;

    RDAddress next = x86_get_ip_value(instr);
    x86_del_regval(ctx, next, dst->reg);
    return true;
}

bool x86_query_reg(RDQueryReg* q, RDProcessor* p) {
    RD_UNUSED(p);

    if(q->kind == RD_QUERY_REG_BY_ID) {
        q->name = ZydisRegisterGetString((ZydisRegister)q->id);
        if(!q->name) return false;
    }
    else if(q->kind == RD_QUERY_REG_BY_NAME) {
        q->id = _x86_get_register_id(q->name);
        if(q->id == RD_REGID_INVALID) return false;
    }
    else
        return false;

    if(q->want & RD_QUERY_REG_WANT_MASK) {
        if(!_x86_get_reg_mask(q->id, &q->mask)) return false;
    }

    if(q->want & RD_QUERY_REG_WANT_CANONICAL) {
        q->canonical_name = ZydisRegisterGetString(_x86_canonical_reg(q->id));
        if(!q->canonical_name) return false;
    }

    return true;
}
