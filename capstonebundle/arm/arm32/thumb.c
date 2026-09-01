#include "thumb.h"
#include "arm/arm32/lifter.h"

const CapstoneInitData THUMB_LE_INIT = {
    .arch = CS_ARCH_ARM,
    .mode = CS_MODE_THUMB | CS_MODE_LITTLE_ENDIAN,
};

const CapstoneInitData THUMB_BE_INIT = {
    .arch = CS_ARCH_ARM,
    .mode = CS_MODE_THUMB | CS_MODE_BIG_ENDIAN,
};

static RDAddress _thumb_get_pc(RDAddress address) {
    return (address & ~3) + sizeof(u32);
}

static RDProcessor* _thumb_create(const RDProcessorPlugin* p) {
    const CapstoneInitData* data = (const CapstoneInitData*)p->userdata;
    return (RDProcessor*)capstone_thumb_create(data);
}

ARMCapstone* capstone_thumb_create(const CapstoneInitData* data) {
    ARMCapstone* self =
        (ARMCapstone*)capstone_create(data, sizeof(ARMCapstone));
    self->get_pc = _thumb_get_pc;
    return self;
}

void capstone_thumb_decode(RDContext* ctx, RDInstruction* instr,
                           RDProcessor* p) {
    char data[sizeof(u32)];
    if(!rd_read(ctx, instr->address, &data, rd_count_of(data))) return;

    const cs_insn* cs_insn =
        capstone_plugin_decode(instr, data, rd_count_of(data), p);
    if(!cs_insn) return;

    const cs_arm* d = &cs_insn->detail->arm;
    instr->write_back = cs_insn->detail->writeback;

    if(capstone_arm32_decode_flow(cs_insn, instr)) return;

    for(int i = 0; i < d->op_count && i < RD_MAX_OPERANDS; i++) {
        const cs_arm_op* cop = &d->operands[i];
        RDOperand* op = &instr->operands[i];

        switch(cop->type) {
            case ARM_OP_REG:
                op->kind = RD_OP_REG;
                op->reg = cop->reg;
                break;

            case ARM_OP_IMM: {
                if(rd_instr_is_branch(instr)) {
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
                    i32 displ = op->displ.offset = cop->subtracted
                                                       ? -(i32)cop->mem.disp
                                                       : (i32)cop->mem.disp;

                    op->kind = RD_OP_MEM;
                    op->mem = _thumb_get_pc(instr->address) + displ;
                }
                else if(cop->mem.base == ARM_REG_INVALID &&
                        cop->mem.index == ARM_REG_INVALID) {
                    op->kind = RD_OP_MEM;
                    op->mem = (RDAddress)cop->mem.disp;
                }
                else {
                    op->kind = RD_OP_DISPL;
                    op->displ.base = cop->mem.base;
                    op->displ.index = cop->mem.index == ARM_REG_INVALID
                                          ? RD_REGID_INVALID
                                          : cop->mem.index;
                    op->displ.offset = cop->subtracted ? -(i32)cop->mem.disp
                                                       : (i32)cop->mem.disp;
                    op->userdata1 = d->post_index;
                }

                break;
            }

            default: break;
        }
    }
}

const RDProcessorPlugin THUMB_LE = {
    .id = "thumb_le",
    .name = "THUMB (Little Endian)",
    .flags = RD_PF_LE,
    .ptr_size = sizeof(u32),
    .userdata = (void*)&THUMB_LE_INIT,
    .create = _thumb_create,
    .destroy = capstone_plugin_destroy,
    .decode = capstone_thumb_decode,
    .emulate = capstone_plugin_arm32_emulate,
    .lift = capstone_arm32_lift,
    .render_operand = capstone_plugin_arm32_render_operand,
    .get_mnemonic = capstone_plugin_get_mnemonic,
    .query_reg = capstone_plugin_query_reg,
};

const RDProcessorPlugin THUMB_BE = {
    .id = "thumb_be",
    .name = "THUMB (Big Endian)",
    .flags = RD_PF_BE,
    .ptr_size = sizeof(u32),
    .userdata = (void*)&THUMB_BE_INIT,
    .create = _thumb_create,
    .destroy = capstone_plugin_destroy,
    .decode = capstone_thumb_decode,
    .emulate = capstone_plugin_arm32_emulate,
    .lift = capstone_arm32_lift,
    .render_operand = capstone_plugin_arm32_render_operand,
    .get_mnemonic = capstone_plugin_get_mnemonic,
    .query_reg = capstone_plugin_query_reg,
};
