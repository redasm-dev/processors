#include "arm32.h"
#include "arm/common.h"
#include "arm/thumb.h"
#include "capstone.h"

typedef struct ARM32Capstone {
    Capstone base;
    Capstone* thumb;
} ARM32Capstone;

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

static RDProcessor* _arm32_create(const RDProcessorPlugin* p) {
    const CapstoneInitData* data = (const CapstoneInitData*)p->userdata;

    ARM32Capstone* self =
        (ARM32Capstone*)capstone_create(data, sizeof(ARM32Capstone));

    if(data == &ARM32_LE_INIT)
        self->thumb = capstone_create(&THUMB_LE_INIT, sizeof(Capstone));
    else
        self->thumb = capstone_create(&THUMB_BE_INIT, sizeof(Capstone));

    return (RDProcessor*)self;
}

static void _arm32_destroy(RDProcessor* p) {
    ARM32Capstone* self = (ARM32Capstone*)p;
    capstone_destroy(self->thumb);
    capstone_destroy((Capstone*)self);
}

void capstone_arm32_decode(RDContext* ctx, RDInstruction* instr,
                           RDProcessor* p) {
    char data[sizeof(u32)];
    if(!rd_read(ctx, instr->address, data, rd_count_of(data))) return;

    const cs_insn* cs_insn =
        capstone_plugin_decode(instr, data, rd_count_of(data), p);
    if(!cs_insn) return;

    const cs_arm* d = &cs_insn->detail->arm;
    instr->write_back = cs_insn->detail->writeback;

    switch(cs_insn->id) {
        case ARM_INS_MOV: { // MOV PC, LR = return
            if(d->operands[0].reg == ARM_REG_PC &&
               d->operands[1].reg == ARM_REG_LR)
                instr->flow = RD_IF_STOP;
            break;
        }

        default: {
            if(capstone_arm32_decode_flow(cs_insn, instr)) return;
            break;
        }
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

                    i32 disp = cop->subtracted ? -(i32)cop->mem.disp
                                               : (i32)cop->mem.disp;

                    op->mem = _arm32_get_pc(instr) + disp;
                }
                else if(cop->mem.index == ARM_REG_INVALID) {
                    op->kind = RD_OP_DISPL;
                    op->displ.base = cop->mem.base;
                    op->displ.index = RD_REGID_UNKNOWN;
                    op->displ.offset = cop->subtracted ? -(i32)cop->mem.disp
                                                       : (i32)cop->mem.disp;
                    op->userdata1 = d->post_index;
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

static void _arm32_decode(RDContext* ctx, RDInstruction* instr,
                          RDProcessor* p) {
    ARM32Capstone* self = (ARM32Capstone*)p;

    u64 t;
    bool is_thumb = rd_get_regval(ctx, instr->address, "T", &t) && t != 0;

    if(is_thumb)
        capstone_thumb_decode(ctx, instr, (RDProcessor*)self->thumb);
    else
        capstone_arm32_decode(ctx, instr, (RDProcessor*)self);
}

static void _arm32_emulate(RDContext* ctx, const RDInstruction* instr,
                           RDProcessor* p) {
    ARM32Capstone* self = (ARM32Capstone*)p;

    u64 t;
    bool is_thumb = rd_get_regval(ctx, instr->address, "T", &t) && t != 0;

    if(instr->id == ARM_INS_BLX) {
        const RDOperand* op = &instr->operands[0];

        if(op->kind == RD_OP_ADDR)
            rd_auto_regval(ctx, op->addr, "T", !is_thumb);
    }

    capstone_plugin_arm32_emulate(ctx, instr, (RDProcessor*)self);
}

const RDProcessorPlugin ARM32_LE = {
    .level = RD_API_LEVEL,
    .id = "arm32_le",
    .name = "ARM32 (Little Endian)",
    .flags = RD_PF_LE,
    .ptr_size = sizeof(u32),
    .int_size = sizeof(u32),
    .userdata = (void*)&ARM32_LE_INIT,
    .create = _arm32_create,
    .destroy = _arm32_destroy,
    .decode = _arm32_decode,
    .emulate = _arm32_emulate,
    .render_operand = capstone_plugin_arm32_render_operand,
    .get_mnemonic = capstone_plugin_get_mnemonic,
    .get_reg_name = capstone_plugin_get_reg_name,
};

const RDProcessorPlugin ARM32_BE = {
    .level = RD_API_LEVEL,
    .id = "arm32_be",
    .name = "ARM32 (Big Endian)",
    .flags = RD_PF_BE,
    .ptr_size = sizeof(u32),
    .int_size = sizeof(u32),
    .userdata = (void*)&ARM32_BE_INIT,
    .create = _arm32_create,
    .destroy = _arm32_destroy,
    .decode = _arm32_decode,
    .emulate = _arm32_emulate,
    .render_operand = capstone_plugin_arm32_render_operand,
    .get_mnemonic = capstone_plugin_get_mnemonic,
    .get_reg_name = capstone_plugin_get_reg_name,
};
