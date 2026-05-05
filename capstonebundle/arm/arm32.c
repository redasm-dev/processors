#include "arm32.h"
#include "arm/common.h"
#include "capstone.h"

static const CapstoneInitData ARM32_LE_INIT = {
    .arch = CS_ARCH_ARM,
    .mode = CS_MODE_ARM | CS_MODE_LITTLE_ENDIAN,
};

static const CapstoneInitData ARM32_BE_INIT = {
    .arch = CS_ARCH_ARM,
    .mode = CS_MODE_ARM | CS_MODE_BIG_ENDIAN,
};

static RDAddress _arm32_get_pc(const RDInstruction* instr) {
    return instr->address + (sizeof(u32) * 2); // ARM PC = address + 8
}

static void _arm32_decode(RDContext* ctx, RDInstruction* instr,
                          RDProcessor* p) {
    char data[sizeof(u32)];
    if(!rd_read(ctx, instr->address, data, rd_count_of(data))) return;

    const cs_insn* cs = capstone_decode(instr, data, rd_count_of(data), p);
    if(!cs) return;

    const cs_arm* d = &cs->detail->arm;

    // flow
    switch(cs->id) {
        case ARM_INS_UDF: instr->flow = RD_IF_STOP; break;

        case ARM_INS_BX: {
            if(d->operands[0].reg == ARM_REG_LR)
                instr->flow = RD_IF_STOP;
            else
                instr->flow = (d->cc == ARMCC_AL || d->cc == ARMCC_Invalid)
                                  ? RD_IF_JUMP
                                  : RD_IF_JUMP_COND;
            break;
        }

        case ARM_INS_B: {
            instr->flow = (d->cc == ARMCC_AL || d->cc == ARMCC_Invalid)
                              ? RD_IF_JUMP
                              : RD_IF_JUMP_COND;
            break;
        }

        case ARM_INS_BL:
        case ARM_INS_BLX: {
            instr->flow = (d->cc == ARMCC_AL || d->cc == ARMCC_Invalid)
                              ? RD_IF_CALL
                              : RD_IF_CALL_COND;
            break;
        }

        case ARM_INS_MOV: {
            // MOV PC, LR = return
            if(d->operands[0].reg == ARM_REG_PC &&
               d->operands[1].reg == ARM_REG_LR)
                instr->flow = RD_IF_STOP;
            break;
        }

        case ARM_INS_POP:
        case ARM_INS_LDM: {
            // POP/LDM with PC in register list = return
            for(int i = 0; i < d->op_count; i++) {
                if(d->operands[i].type == ARM_OP_REG &&
                   d->operands[i].reg == ARM_REG_PC) {
                    instr->flow = RD_IF_STOP;
                    break;
                }
            }

            break;
        }

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

            case ARM_OP_IMM:
                if(rd_is_branch(instr)) {
                    op->kind = RD_OP_ADDR;
                    op->addr = (RDAddress)(cop->imm & ~1);
                }
                else {
                    op->kind = RD_OP_IMM;
                    op->s_imm = cop->imm;
                }
                break;

            case ARM_OP_MEM:
                if(cop->mem.base == ARM_REG_PC &&
                   cop->mem.index == ARM_REG_INVALID) {
                    op->kind = RD_OP_MEM;
                    op->mem = _arm32_get_pc(instr) + cop->mem.disp;
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

            default: break;
        }
    }
}

const RDProcessorPlugin ARM32_LE = {
    .level = RD_API_LEVEL,
    .id = "arm32_le",
    .name = "ARM32 (Little Endian)",
    .flags = RD_PF_LE,
    .ptr_size = sizeof(u32),
    .int_size = sizeof(u32),
    .userdata = (void*)&ARM32_LE_INIT,
    .create = capstone_create,
    .destroy = capstone_destroy,
    .decode = _arm32_decode,
    .emulate = capstone_arm32_emulate,
    .render_operand = capstone_arm32_render_operand,
    .get_mnemonic = capstone_get_mnemonic,
    .get_reg_name = capstone_get_reg_name,
};

const RDProcessorPlugin ARM32_BE = {
    .level = RD_API_LEVEL,
    .id = "arm32_be",
    .name = "ARM32 (Big Endian)",
    .flags = RD_PF_BE,
    .ptr_size = sizeof(u32),
    .int_size = sizeof(u32),
    .userdata = (void*)&ARM32_BE_INIT,
    .create = capstone_create,
    .destroy = capstone_destroy,
    .decode = _arm32_decode,
    .emulate = capstone_arm32_emulate,
    .render_operand = capstone_arm32_render_operand,
    .get_mnemonic = capstone_get_mnemonic,
    .get_reg_name = capstone_get_reg_name,
};
