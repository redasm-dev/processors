#include "thumb.h"
#include "arm/common.h"
#include "capstone.h"

static const CapstoneInitData THUMB_LE_INIT = {
    .arch = CS_ARCH_ARM,
    .mode = CS_MODE_THUMB | CS_MODE_LITTLE_ENDIAN,
};

static const CapstoneInitData THUMB_BE_INIT = {
    .arch = CS_ARCH_ARM,
    .mode = CS_MODE_THUMB | CS_MODE_BIG_ENDIAN,
};

static RDAddress _thumb_get_pc(const RDInstruction* instr) {
    return (instr->address & ~3) + sizeof(u32);
}

static void _thumb_decode(RDContext* ctx, RDInstruction* instr,
                          RDProcessor* p) {
    char data[sizeof(u32)];
    if(!rd_read(ctx, instr->address, &data, rd_count_of(data))) return;

    const cs_insn* cs_insn = capstone_decode(instr, data, rd_count_of(data), p);
    if(!cs_insn) return;

    const cs_arm* d = &cs_insn->detail->arm;

    switch(instr->id) {
        case ARM_INS_UDF: instr->flow = RD_IF_STOP; break;

        case ARM_INS_BX: {
            if(d->operands[0].reg != ARM_REG_LR) {
                instr->flow = (d->cc == ARMCC_AL || d->cc == ARMCC_Invalid)
                                  ? RD_IF_JUMP
                                  : RD_IF_JUMP_COND;
            }
            else
                instr->flow = RD_IF_STOP;

            break;
        }

        case ARM_INS_POP: {
            for(uint8_t i = 0; i < d->op_count; i++) {
                if(d->operands[i].reg == ARM_REG_PC) {
                    instr->flow = RD_IF_STOP;
                    break;
                }
            }

            break;
        }

        case ARM_INS_B: {
            instr->flow = (d->cc == ARMCC_AL || d->cc == ARMCC_Invalid)
                              ? RD_IF_JUMP
                              : RD_IF_JUMP_COND;
            break;
        }

        case ARM_INS_BL:
        case ARM_INS_BLX: instr->flow = RD_IF_CALL; break;

        default: break;
    }

    for(int i = 0; i < d->op_count && i < RD_MAX_OPERANDS; i++) {
        const cs_arm_op* cop = &d->operands[i];
        RDOperand* op = &instr->operands[i];

        switch(cop->type) {
            case ARM_OP_REG:
                op->kind = RD_OP_REG;
                op->reg = cop->reg;
                break;

            case ARM_OP_IMM: {
                if(rd_is_branch(instr)) {
                    op->kind = RD_OP_ADDR;
                    op->addr = (RDAddress)(cop->imm & ~1); // strip LSB
                }
                else {
                    op->kind = RD_OP_IMM;
                    op->s_imm = cop->imm;
                }

                break;
            }

            case ARM_OP_MEM: {
                if(cop->mem.base == ARM_REG_PC &&
                   cop->mem.index == ARM_REG_INVALID) {
                    // PC-relative literal pool
                    // ARM PC = current & ~3 + sizeof(u32)
                    op->kind = RD_OP_MEM;
                    op->mem = _thumb_get_pc(instr) + cop->mem.disp;
                }
                else if(cop->mem.base == ARM_REG_INVALID &&
                        cop->mem.index == ARM_REG_INVALID) {
                    op->kind = RD_OP_MEM;
                    op->mem = (RDAddress)cop->mem.disp;
                }
                else if(cop->mem.index == ARM_REG_INVALID) {
                    op->kind = RD_OP_DISPL;
                    op->displ.base = cop->mem.base;
                    op->displ.index = RD_REGID_UNKNOWN;
                    op->displ.offset = cop->mem.disp;
                }
                else {
                    op->kind = RD_OP_PHRASE;
                    op->phrase.base = cop->mem.base;
                    op->phrase.index = cop->mem.index;
                }

                break;
            }

            default: break;
        }
    }
}

const RDProcessorPlugin THUMB_LE = {
    .level = RD_API_LEVEL,
    .id = "thumb_le",
    .name = "THUMB (Little Endian)",
    .flags = RD_PF_LE,
    .ptr_size = sizeof(u32),
    .int_size = sizeof(u32),
    .userdata = (void*)&THUMB_LE_INIT,
    .create = capstone_create,
    .destroy = capstone_destroy,
    .decode = _thumb_decode,
    .emulate = capstone_arm32_emulate,
    .render_operand = capstone_arm32_render_operand,
    .get_mnemonic = capstone_get_mnemonic,
    .get_reg_name = capstone_get_reg_name,
};

const RDProcessorPlugin THUMB_BE = {
    .level = RD_API_LEVEL,
    .id = "thumb_be",
    .name = "THUMB (Big Endian)",
    .flags = RD_PF_BE,
    .ptr_size = sizeof(u32),
    .int_size = sizeof(u32),
    .userdata = (void*)&THUMB_BE_INIT,
    .create = capstone_create,
    .destroy = capstone_destroy,
    .emulate = capstone_arm32_emulate,
    .render_operand = capstone_arm32_render_operand,
    .get_mnemonic = capstone_get_mnemonic,
    .get_reg_name = capstone_get_reg_name,
};
