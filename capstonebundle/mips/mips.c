#include "mips.h"
#include "capstone.h"
#include "mips/fallback.h"

static const CapstoneInitData MIPS32_LE_INIT = {
    .arch = CS_ARCH_MIPS,
    .mode = CS_MODE_MIPS32 | CS_MODE_LITTLE_ENDIAN,
    .reg_bits = 32,
};

static const CapstoneInitData MIPS32_BE_INIT = {
    .arch = CS_ARCH_MIPS,
    .mode = CS_MODE_MIPS32 | CS_MODE_BIG_ENDIAN,
    .reg_bits = 32,
};

static const CapstoneInitData MIPS64_LE_INIT = {
    .arch = CS_ARCH_MIPS,
    .mode = CS_MODE_MIPS64 | CS_MODE_LITTLE_ENDIAN,
    .reg_bits = 64,
};

static const CapstoneInitData MIPS64_BE_INIT = {
    .arch = CS_ARCH_MIPS,
    .mode = CS_MODE_MIPS64 | CS_MODE_BIG_ENDIAN,
    .reg_bits = 64,
};

static bool _mips_has_delay_slot(const cs_insn* insn) {
    if(insn->is_alias) {
        switch(insn->alias_id) {
            case MIPS_INS_ALIAS_B:
            case MIPS_INS_ALIAS_BAL:
            case MIPS_INS_ALIAS_BEQZ:
            case MIPS_INS_ALIAS_BNEZ: return true;

            default: break;
        }
    }

    switch(insn->id) {
        case MIPS_INS_BEQ:
        case MIPS_INS_BGEZ:
        case MIPS_INS_BGTZ:
        case MIPS_INS_BLEZ:
        case MIPS_INS_BLTZ:
        case MIPS_INS_BLTZAL:
        case MIPS_INS_BGEZAL:
        case MIPS_INS_BNE:
        case MIPS_INS_J:
        case MIPS_INS_JAL:
        case MIPS_INS_JALR:
        case MIPS_INS_JR: return true;

        default: break;
    }

    return false;
}

// instructions whose operand 0 is only read
static bool _mips_op0_is_read(const RDInstruction* instr) {
    if(rd_instr_is_branch(instr)) return true;

    switch(instr->id) {
        case MIPS_INS_SB:
        case MIPS_INS_SH:
        case MIPS_INS_SW:
        case MIPS_INS_SWL:
        case MIPS_INS_SWR:
        case MIPS_INS_SWC2:
        case MIPS_INS_MTC0:
        case MIPS_INS_MTC2:
        case MIPS_INS_CTC2:
        case MIPS_INS_MTHI:
        case MIPS_INS_MTLO:
        case MIPS_INS_MULT:
        case MIPS_INS_MULTU:
        case MIPS_INS_DIV:
        case MIPS_INS_DIVU:
        case MIPS_X_INS_COP2: return true;

        default: return false;
    }
}

static void _mips_forget_op0(RDContext* ctx, const RDInstruction* instr) {
    const RDOperand* op = &instr->operands[0];

    if(op->kind == RD_OP_REG && op->reg != MIPS_REG_ZERO)
        rd_del_regval_id(ctx, op->reg);
}

static void _mips_render_reg(RDRenderer* r, RDReg reg, RDProcessor* p) {
    const Capstone* cs = (const Capstone*)p;
    rd_renderer_fmt(r, RD_THEME_REG, "$%s", cs_reg_name(cs->handle, reg));
}

static void _mips_handle_operands(RDContext* ctx, const RDInstruction* instr) {
    rd_foreach_operand(i, op, instr) {
        switch(op->kind) {
            case RD_OP_ADDR: {
                if(rd_instr_is_call(instr))
                    rd_auto_xref_add(ctx, instr->address, op->addr, RD_CR_CALL);
                else if(rd_instr_is_jump(instr))
                    rd_auto_xref_add(ctx, instr->address, op->addr, RD_CR_JUMP);
                break;
            };

            default: break;
        }
    }
}

// a stack/frame based value is never a reference
static bool _mips_is_stack_reg(RDReg r) {
    return r == MIPS_REG_SP || r == MIPS_REG_FP;
}

// lui+addiu / lui+ori: the instruction completing a value that is an address
static void _mips_ref_composed(RDContext* ctx, const RDInstruction* instr) {
    const RDOperand* dst = &instr->operands[0];
    const RDOperand* src = &instr->operands[1];

    if(dst->kind != RD_OP_REG || src->kind != RD_OP_REG) return;
    if(_mips_is_stack_reg(dst->reg) || _mips_is_stack_reg(src->reg)) return;

    RDRegValue v;
    if(rd_get_regval_id(ctx, dst->reg, &v) && rd_is_address(ctx, (RDAddress)v))
        rd_auto_xref_add(ctx, instr->address, (RDAddress)v, RD_DR_ADDRESS);
}

// $zero always reads as 0: pass a constant, never the register
static void _mips_alu_src(RDAluOperand* s, const RDInstruction* instr,
                          usize idx) {
    const RDOperand* op = &instr->operands[idx];

    if(op->kind == RD_OP_REG && op->reg == MIPS_REG_ZERO) {
        s->kind = RD_ALU_CONSTANT;
        s->cnst = 0;
    }
    else {
        s->kind = RD_ALU_INDEX;
        s->index = idx;
    }
}

// dst = op1 <op> op2
static void _mips_alu3(RDAlu* alu, const RDInstruction* instr, RDAluOp op) {
    alu->op = op;
    alu->dst.kind = RD_ALU_INDEX;
    alu->dst.index = 0;
    _mips_alu_src(&alu->src1, instr, 1);
    _mips_alu_src(&alu->src2, instr, 2);
}

static void _mips_emu_alu(RDContext* ctx, const RDInstruction* instr) {
    const RDOperand* dst = &instr->operands[0];

    if(dst->kind != RD_OP_REG ||
       (dst->kind == RD_OP_REG && dst->reg == MIPS_REG_ZERO))
        return;

    RDAlu alu = {0};

    switch(instr->id) {
        case MIPS_INS_ADD:
        case MIPS_INS_ADDU:
        case MIPS_INS_ADDI:
        case MIPS_INS_ADDIU: _mips_alu3(&alu, instr, RD_ALUOP_ADD); break;

        case MIPS_INS_SUB:
        case MIPS_INS_SUBU: _mips_alu3(&alu, instr, RD_ALUOP_SUB); break;

        case MIPS_INS_AND:
        case MIPS_INS_ANDI: _mips_alu3(&alu, instr, RD_ALUOP_AND); break;

        case MIPS_INS_OR:
        case MIPS_INS_ORI: _mips_alu3(&alu, instr, RD_ALUOP_OR); break;

        case MIPS_INS_XOR:
        case MIPS_INS_XORI: _mips_alu3(&alu, instr, RD_ALUOP_XOR); break;

        case MIPS_INS_SLL:
        case MIPS_INS_SLLV: _mips_alu3(&alu, instr, RD_ALUOP_LSL); break;

        case MIPS_INS_SRL:
        case MIPS_INS_SRLV: _mips_alu3(&alu, instr, RD_ALUOP_LSR); break;

        case MIPS_INS_SRA:
        case MIPS_INS_SRAV: _mips_alu3(&alu, instr, RD_ALUOP_ASR); break;

        case MIPS_INS_LUI: // rt = imm << 16
            alu.op = RD_ALUOP_LSL;
            alu.dst.kind = RD_ALU_INDEX;
            alu.dst.index = 0;
            alu.src1.kind = RD_ALU_INDEX;
            alu.src1.index = 1;
            alu.src2.kind = RD_ALU_CONSTANT;
            alu.src2.cnst = 16;
            break;

        default: break;
    }

    if(rd_alu(ctx, instr, &alu) != RD_ALU_DONE) {
        _mips_forget_op0(ctx, instr);
        return;
    }

    switch(instr->id) {
        case MIPS_INS_ADDI:
        case MIPS_INS_ADDIU:
        case MIPS_INS_ORI: _mips_ref_composed(ctx, instr); break;

        default: break;
    }
}

static void mips_decode(RDContext* ctx, RDInstruction* instr, RDProcessor* p) {
    char data[sizeof(u32)];
    if(!rd_read_exact(ctx, instr->address, data, rd_count_of(data))) return;

    const cs_insn* insn = capstone_plugin_decode(instr, data, sizeof(data), p);

    if(!insn) {
        mips_fallback_decode(data, (const Capstone*)p, instr);
        return;
    }

    if(_mips_has_delay_slot(insn)) instr->delay_slots = 1;

    const cs_mips* d = &insn->detail->mips;
    if(insn->is_alias && insn->alias_id == MIPS_INS_ALIAS_B) {
        instr->flow = RD_IF_JUMP;
    }
    else if(insn->is_alias && insn->alias_id == MIPS_INS_ALIAS_BAL) {
        instr->flow = RD_IF_CALL;
    }
    else {
        switch(insn->id) {
            case MIPS_INS_J: instr->flow = RD_IF_JUMP; break;

            case MIPS_INS_BGT:
            case MIPS_INS_BLT:
            case MIPS_INS_BLE:
            case MIPS_INS_BGE:
            case MIPS_INS_BNE:
            case MIPS_INS_BEQ:
            case MIPS_INS_BGTZ:
            case MIPS_INS_BLTZ:
            case MIPS_INS_BLEZ:
            case MIPS_INS_BGEZ: instr->flow = RD_IF_JUMP_COND; break;

            case MIPS_INS_BAL:
            case MIPS_INS_JAL:
            case MIPS_INS_JALR: instr->flow = RD_IF_CALL; break;

            case MIPS_INS_BLTZAL:
            case MIPS_INS_BGEZAL: instr->flow = RD_IF_CALL_COND; break;

            case MIPS_INS_JR: {
                if(d->operands[0].type == MIPS_OP_REG) {
                    instr->flow = d->operands[0].reg == MIPS_REG_RA
                                      ? RD_IF_STOP
                                      : RD_IF_JUMP;
                }

                break;
            }

            case MIPS_INS_SLL: {
                if(insn->is_alias && insn->alias_id == MIPS_INS_ALIAS_NOP)
                    instr->flow = RD_IF_NOP;

                break;
            }

            case MIPS_INS_BREAK:
            case MIPS_INS_SDBBP: instr->flow = RD_IF_STOP; break;

            default: break;
        }
    }

    for(u8 i = 0; i < d->op_count && i < RD_MAX_OPERANDS; i++) {
        const cs_mips_op* cop = &d->operands[i];
        RDOperand* op = &instr->operands[i];

        switch(cop->type) {
            case MIPS_OP_REG: {
                op->kind = RD_OP_REG;
                op->reg = cop->reg;
                break;
            }

            case MIPS_OP_IMM: {
                if(rd_instr_is_branch(instr)) {
                    op->kind = RD_OP_ADDR;
                    op->addr = (RDAddress)cop->imm;
                }
                else {
                    op->kind = RD_OP_IMM;
                    op->imm = cop->imm;
                }

                break;
            }

            case MIPS_OP_MEM: {
                op->kind = RD_OP_DISPL;
                op->displ.base = cop->mem.base;
                op->displ.offset = cop->mem.disp;
                break;
            }

            default: op->kind = RD_OP_STUB; break;
        }
    }
}

static void mips_emulate(RDContext* ctx, const RDInstruction* instr,
                         RDProcessor* p) {
    RD_UNUSED(p);

    switch(instr->id) {
        case MIPS_INS_ADD:
        case MIPS_INS_ADDU:
        case MIPS_INS_ADDI:
        case MIPS_INS_ADDIU:
        case MIPS_INS_SUB:
        case MIPS_INS_SUBU:
        case MIPS_INS_AND:
        case MIPS_INS_ANDI:
        case MIPS_INS_OR:
        case MIPS_INS_ORI:
        case MIPS_INS_XOR:
        case MIPS_INS_XORI:
        case MIPS_INS_SLL:
        case MIPS_INS_SLLV:
        case MIPS_INS_SRL:
        case MIPS_INS_SRLV:
        case MIPS_INS_SRA:
        case MIPS_INS_SRAV:
        case MIPS_INS_LUI: _mips_emu_alu(ctx, instr); break;

        default: {
            _mips_handle_operands(ctx, instr);
            if(!_mips_op0_is_read(instr)) _mips_forget_op0(ctx, instr);
            break;
        }
    }

    if(rd_instr_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

static bool mips_render_operand(RDRenderer* r, const RDInstruction* instr,
                                int idx, RDProcessor* p) {

    const RDOperand* op = &instr->operands[idx];

    if(op->kind == RD_OP_REG) {
        _mips_render_reg(r, op->reg, p);
        return true;
    }

    if(op->kind == RD_OP_DISPL) {
        rd_renderer_num(r, op->displ.offset, 16, 0, 0);
        rd_renderer_norm(r, "(");
        _mips_render_reg(r, op->displ.base, p);
        rd_renderer_norm(r, ")");
        return true;
    }

    return false;
}

const RDProcessorPlugin MIPS32_LE = {
    .id = "mips32_le",
    .name = "MIPS32 (Little Endian)",
    .flags = RD_PF_LE,
    .ptr_size = sizeof(u32),
    .userdata = (void*)&MIPS32_LE_INIT,
    .create = capstone_plugin_create,
    .destroy = capstone_plugin_destroy,
    .decode = mips_decode,
    .emulate = mips_emulate,
    .get_mnemonic = capstone_plugin_get_mnemonic,
    .query_reg = capstone_plugin_query_reg,
    .render_operand = mips_render_operand,
};

const RDProcessorPlugin MIPS32_BE = {
    .id = "mips32_be",
    .name = "MIPS32 (Big Endian)",
    .flags = RD_PF_BE,
    .ptr_size = sizeof(u32),
    .userdata = (void*)&MIPS32_BE_INIT,
    .create = capstone_plugin_create,
    .destroy = capstone_plugin_destroy,
    .decode = mips_decode,
    .emulate = mips_emulate,
    .get_mnemonic = capstone_plugin_get_mnemonic,
    .query_reg = capstone_plugin_query_reg,
    .render_operand = mips_render_operand,
};

const RDProcessorPlugin MIPS64_LE = {
    .id = "mips64_le",
    .name = "MIPS64 (Little Endian)",
    .flags = RD_PF_LE,
    .ptr_size = sizeof(u64),
    .userdata = (void*)&MIPS64_LE_INIT,
    .create = capstone_plugin_create,
    .destroy = capstone_plugin_destroy,
    .decode = mips_decode,
    .emulate = mips_emulate,
    .get_mnemonic = capstone_plugin_get_mnemonic,
    .query_reg = capstone_plugin_query_reg,
    .render_operand = mips_render_operand,
};

const RDProcessorPlugin MIPS64_BE = {
    .id = "mips64_be",
    .name = "MIPS64 (Big Endian)",
    .flags = RD_PF_BE,
    .ptr_size = sizeof(u64),
    .userdata = (void*)&MIPS64_BE_INIT,
    .create = capstone_plugin_create,
    .destroy = capstone_plugin_destroy,
    .decode = mips_decode,
    .emulate = mips_emulate,
    .get_mnemonic = capstone_plugin_get_mnemonic,
    .query_reg = capstone_plugin_query_reg,
    .render_operand = mips_render_operand,
};
