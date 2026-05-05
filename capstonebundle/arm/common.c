#include "common.h"

void capstone_arm32_emulate(RDContext* ctx, const RDInstruction* instr,
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
        else if(op->kind == RD_OP_MEM && rd_is_address(ctx, op->mem)) {
            rd_add_xref(ctx, instr->address, op->mem, RD_DR_READ);

            const RDOperand* dst = &instr->operands[0];
            const RDOperand* src = &instr->operands[1];
            if(dst->kind == RD_OP_REG && src->kind == RD_OP_MEM)
                rd_auto_type(ctx, src->mem, "u32", 0, RD_TYPE_PTR);
        }
    }

    if(rd_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

bool capstone_arm32_render_operand(RDRenderer* r, const RDInstruction* instr,
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
