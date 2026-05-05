#include "arm64.h"
#include "capstone.h"

static const CapstoneInitData ARM64_LE_INIT = {
    .arch = CS_ARCH_AARCH64,
    .mode = CS_MODE_ARM | CS_MODE_LITTLE_ENDIAN,
};

static const CapstoneInitData ARM64_BE_INIT = {
    .arch = CS_ARCH_AARCH64,
    .mode = CS_MODE_ARM | CS_MODE_BIG_ENDIAN,
};

static void _arm64_decode(RDContext* ctx, RDInstruction* instr,
                          RDProcessor* p) {
    char data[sizeof(u32)];
    if(!rd_read(ctx, instr->address, &data, rd_count_of(data))) return;

    const cs_insn* cs_insn =
        capstone_plugin_decode(instr, data, rd_count_of(data), p);
    if(!cs_insn) return;

    const cs_aarch64* d = &cs_insn->detail->aarch64;

    switch(instr->id) {
        case AARCH64_INS_B: {
            instr->flow = d->cc == AArch64CC_AL ? RD_IF_JUMP : RD_IF_JUMP_COND;
            break;
        }

        case AARCH64_INS_BR: instr->flow = RD_IF_JUMP; break;

        case AARCH64_INS_CBZ:
        case AARCH64_INS_CBNZ:
        case AARCH64_INS_TBZ:
        case AARCH64_INS_TBNZ: instr->flow = RD_IF_JUMP_COND; break;

        case AARCH64_INS_BL:
        case AARCH64_INS_BLR:
        case AARCH64_INS_BLRAA:
        case AARCH64_INS_BLRAB:
        case AARCH64_INS_BLRAAZ:
        case AARCH64_INS_BLRABZ: instr->flow = RD_IF_CALL; break;

        case AARCH64_INS_UDF:
        case AARCH64_INS_RET:
        case AARCH64_INS_BRK:
        case AARCH64_INS_HLT:
        case AARCH64_INS_ERET:
        case AARCH64_INS_ERETAA:
        case AARCH64_INS_ERETAB: instr->flow = RD_IF_STOP; break;

        default: break;
    }

    for(uint8_t i = 0; i < d->op_count; i++) {
        const cs_aarch64_op* cs_op = &d->operands[i];
        RDOperand* op = &instr->operands[i];

        switch(cs_op->type) {
            case AARCH64_OP_REG:
                instr->operands[i].kind = RD_OP_REG;
                instr->operands[i].reg = cs_op->reg;
                break;

            case AARCH64_OP_IMM: {
                if(rd_is_branch(instr)) {
                    instr->operands[i].kind = RD_OP_ADDR;
                    instr->operands[i].addr = (RDAddress)cs_op->imm;
                }
                else {
                    instr->operands[i].kind = RD_OP_IMM;
                    instr->operands[i].s_imm = cs_op->imm;
                }
                break;
            }

            case AARCH64_OP_MEM: {
                if(cs_op->mem.base == AARCH64_REG_INVALID &&
                   cs_op->mem.index == AARCH64_REG_INVALID) {
                    // PC-relative literal (LDR Xn, label)
                    op->kind = RD_OP_MEM;
                    op->mem = (RDAddress)cs_op->mem.disp;
                }
                else if(cs_op->mem.index == AARCH64_REG_INVALID &&
                        cs_op->mem.base != AARCH64_REG_INVALID) {
                    op->kind = RD_OP_DISPL;
                    op->displ.base = cs_op->mem.base;
                    op->displ.index = RD_REGID_UNKNOWN;
                    op->displ.offset = cs_op->mem.disp;
                }
                else {
                    op->kind = RD_OP_PHRASE;
                    op->phrase.base = cs_op->mem.base;
                    op->phrase.index = cs_op->mem.index;
                }
                break;
            }

            default: break;
        }
    }
}

static void _arm64_emulate(RDContext* ctx, const RDInstruction* instr,
                           RDProcessor* p) {
    RD_UNUSED(p);

    rd_foreach_operand(i, op, instr) {
        if(op->kind == RD_OP_ADDR) {
            if(rd_is_call(instr))
                rd_add_xref(ctx, instr->address, op->addr, RD_CR_CALL);
            else if(rd_is_jump(instr))
                rd_add_xref(ctx, instr->address, op->addr, RD_CR_JUMP);
            else
                rd_add_xref(ctx, instr->address, op->addr, RD_DR_ADDRESS);
        }
        else if(op->kind == RD_OP_MEM && rd_is_address(ctx, op->mem))
            rd_add_xref(ctx, instr->address, op->mem, RD_DR_READ);
    }

    // catch pc-relative loading
    const RDOperand* dst = &instr->operands[0];
    const RDOperand* src = &instr->operands[1];

    if(dst->kind == RD_OP_REG && src->kind == RD_OP_MEM)
        rd_auto_type(ctx, src->mem, "u64", 0, RD_TYPE_PTR);

    if(rd_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

static bool _arm64_render_operand(RDRenderer* r, const RDInstruction* instr,
                                  usize idx, RDProcessor* p) {
    RD_UNUSED(p);
    const RDOperand* op = &instr->operands[idx];

    if(op->kind == RD_OP_DISPL) {
        rd_renderer_norm(r, "[");
        rd_renderer_reg(r, op->displ.base);

        if(op->displ.offset != 0) {
            rd_renderer_norm(r, ", #");
            rd_renderer_num(r, op->displ.offset, 10, 0, RD_NUM_SIGNED);
        }

        rd_renderer_norm(r, "]");
        return true;
    }

    if(op->kind == RD_OP_PHRASE) {
        rd_renderer_norm(r, "[");
        rd_renderer_reg(r, op->phrase.base);
        rd_renderer_norm(r, ", ");
        rd_renderer_reg(r, op->phrase.index);
        rd_renderer_norm(r, "]");
        return true;
    }

    return false;
}

const RDProcessorPlugin ARM64_LE = {
    .level = RD_API_LEVEL,
    .id = "arm64_le",
    .name = "ARM64 (Little Endian)",
    .flags = RD_PF_LE,
    .ptr_size = sizeof(u64),
    .int_size = sizeof(u32),
    .userdata = (void*)&ARM64_LE_INIT,
    .create = capstone_plugin_create,
    .destroy = capstone_plugin_destroy,
    .decode = _arm64_decode,
    .emulate = _arm64_emulate,
    .render_operand = _arm64_render_operand,
    .get_mnemonic = capstone_plugin_get_mnemonic,
    .get_reg_name = capstone_plugin_get_reg_name,
};

const RDProcessorPlugin ARM64_BE = {
    .level = RD_API_LEVEL,
    .id = "arm64_be",
    .name = "ARM64 (Big Endian)",
    .flags = RD_PF_BE,
    .ptr_size = sizeof(u64),
    .int_size = sizeof(u32),
    .userdata = (void*)&ARM64_BE_INIT,
    .create = capstone_plugin_create,
    .destroy = capstone_plugin_destroy,
    .decode = _arm64_decode,
    .emulate = _arm64_emulate,
    .render_operand = _arm64_render_operand,
    .get_mnemonic = capstone_plugin_get_mnemonic,
    .get_reg_name = capstone_plugin_get_reg_name,
};
