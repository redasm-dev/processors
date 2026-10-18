#include "common.h"

typedef enum {
    ARM_OP_REGMASK = RD_OP_USERBASE,
    ARM_OP_PSR,
} ARMOperands;

static int _capstone_arm32_reg_to_index(arm_reg reg) {
    switch(reg) {
        case ARM_REG_R0: return 0;
        case ARM_REG_R1: return 1;
        case ARM_REG_R2: return 2;
        case ARM_REG_R3: return 3;
        case ARM_REG_R4: return 4;
        case ARM_REG_R5: return 5;
        case ARM_REG_R6: return 6;
        case ARM_REG_R7: return 7;
        case ARM_REG_R8: return 8;
        case ARM_REG_SB: return 9;
        case ARM_REG_SL: return 10;
        case ARM_REG_FP: return 11;
        case ARM_REG_IP: return 12;
        case ARM_REG_SP: return 13;
        case ARM_REG_LR: return 14;
        case ARM_REG_PC: return 15;
        default: return -1;
    }
}

static arm_reg _capstone_arm32_index_to_reg(int idx) {
    switch(idx) {
        case 0: return ARM_REG_R0;
        case 1: return ARM_REG_R1;
        case 2: return ARM_REG_R2;
        case 3: return ARM_REG_R3;
        case 4: return ARM_REG_R4;
        case 5: return ARM_REG_R5;
        case 6: return ARM_REG_R6;
        case 7: return ARM_REG_R7;
        case 8: return ARM_REG_R8;
        case 9: return ARM_REG_SB;
        case 10: return ARM_REG_SL;
        case 11: return ARM_REG_FP;
        case 12: return ARM_REG_IP;
        case 13: return ARM_REG_SP;
        case 14: return ARM_REG_LR;
        case 15: return ARM_REG_PC;
        default: return ARM_REG_INVALID;
    }
}

static bool _capstone_arm32_get_regval(RDContext* ctx, RDAddress address,
                                       arm_reg reg, RDRegValue* val,
                                       const ARMCapstone* p) {
    if(rd_get_regval_id(ctx, reg, val)) return true;

    if(reg == ARM_REG_PC) {
        *val = p->get_pc(address);
        return true;
    }

    return false;
}

static void _capstone_arm32_set_regval(RDContext* ctx, arm_reg reg,
                                       RDRegValue val) {
    rd_set_regval_id(ctx, reg, val);
}

static bool _capstone_arm32_writes_pc(const RDInstruction* instr) {
    const RDOperand* op = &instr->operands[0];
    return op->kind == RD_OP_REG && op->reg == ARM_REG_PC && op->userdata2;
}

static bool _capstone_arm32_is_call(RDContext* ctx,
                                    const RDInstruction* instr) {
    if(rd_instr_is_call(instr)) return true;

    if(!rd_instr_is_jump(instr) && !_capstone_arm32_writes_pc(instr))
        return false;

    RDRegValue lr;
    return rd_get_regval_id(ctx, ARM_REG_LR, &lr) &&
           ((u32)lr & ~1U) == (u32)(instr->address + instr->length);
}

static bool _capstone_arm32_is_load_multiple(unsigned id) {
    switch(id) {
        case ARM_INS_LDM:
        case ARM_INS_LDMDA:
        case ARM_INS_LDMDB:
        case ARM_INS_LDMIB:
        case ARM_INS_POP: return true;
        default: return false;
    }
}

static bool _capstone_arm32_is_block_transfer(unsigned id) {
    switch(id) {
        case ARM_INS_STM:
        case ARM_INS_STMDA:
        case ARM_INS_STMDB:
        case ARM_INS_STMIB:
        case ARM_INS_LDM:
        case ARM_INS_LDMDA:
        case ARM_INS_LDMDB:
        case ARM_INS_LDMIB: return true;
        default: return false;
    }
}

static void _capstone_arm32_set_return(const cs_arm* d, RDInstruction* instr) {
    bool always = d->cc == ARMCC_AL || d->cc == ARMCC_Invalid;
    // conditional return falls through
    instr->flow = always ? RD_IF_STOP : RD_IF_NONE;
}

static void _capstone_arm32_branch_to(RDContext* ctx,
                                      const RDInstruction* instr, u32 value,
                                      bool interwork) {
    bool to_thumb = interwork && (value & 1);
    RDAddress addr = value & ~1U;

    if(!interwork && (value & 1)) return;
    if(!rd_is_executable(ctx, addr)) return;
    if(!to_thumb && (addr & 3)) return;

    // the target comes from a register.
    // never change the mode of code that is already decoded
    if(!rd_is_code(ctx, addr)) rd_auto_sregval(ctx, addr, "T", to_thumb);

    rd_add_xref(ctx, instr->address, addr,
                _capstone_arm32_is_call(ctx, instr) ? RD_CR_CALL : RD_CR_JUMP);
}

static bool _capstone_arm32_read_const32(RDContext* ctx, RDAddress addr,
                                         u32* val) {
    const RDProcessorPlugin* plugin = rd_get_processor_plugin(ctx);
    bool is_be = plugin->flags & RD_PF_BE;

    if(rd_is_writable(ctx, addr)) return false;
    return is_be ? rd_read_be32(ctx, addr, val) : rd_read_le32(ctx, addr, val);
}

static void _capstone_arm32_clear_written(RDContext* ctx,
                                          const RDInstruction* instr) {
    rd_foreach_operand(i, op, instr) {
        if(op->kind == RD_OP_REG &&
           (op->userdata2 || (i == 0 && instr->write_back &&
                              _capstone_arm32_is_block_transfer(instr->id))))
            rd_del_regval_id(ctx, op->reg);
        else if(op->kind == RD_OP_DISPL && instr->write_back)
            rd_del_regval_id(ctx, op->displ.base);
        else if(op->kind == ARM_OP_REGMASK &&
                _capstone_arm32_is_load_multiple(instr->id)) {
            for(int r = 0; r < 16; r++)
                if(op->imm & (1U << r))
                    rd_del_regval_id(ctx, _capstone_arm32_index_to_reg(r));
        }
    }
}

static void _capstone_arm32_clobber(RDContext* ctx, bool with_lr) {
    static const arm_reg REGS[] = {
        ARM_REG_R0, ARM_REG_R1, ARM_REG_R2, ARM_REG_R3, ARM_REG_IP,
    };

    for(usize i = 0; i < rd_count_of(REGS); i++)
        rd_del_regval_id(ctx, REGS[i]);

    if(with_lr) rd_del_regval_id(ctx, ARM_REG_LR);
}

static void _capstone_arm32_track(RDContext* ctx, const RDInstruction* instr,
                                  RDProcessor* p) {
    const ARMCapstone* capstone = (const ARMCapstone*)p;

    switch(instr->id) {
        case ARM_INS_ADD:
        case ARM_INS_SUB: {
            const RDOperand* dst = &instr->operands[0];
            const RDOperand* src = &instr->operands[1];

            // simplified adr
            if(dst->kind == RD_OP_REG && src->kind == RD_OP_ADDR) {
                _capstone_arm32_set_regval(ctx, dst->reg,
                                           (u32)(src->addr | src->userdata1));
            }
            else // ordinary add/sub: result unknown
                _capstone_arm32_clear_written(ctx, instr);

            break;
        }

        case ARM_INS_MOV: {
            const RDOperand* dst = &instr->operands[0];
            const RDOperand* src = &instr->operands[1];

            if(dst->kind == RD_OP_REG && dst->reg == ARM_REG_PC) {
                // mov pc, rN: follow the target if known.
                RDRegValue val;
                if(src->kind == RD_OP_REG && src->reg != ARM_REG_LR &&
                   _capstone_arm32_get_regval(ctx, instr->address, src->reg,
                                              &val, capstone))
                    _capstone_arm32_branch_to(ctx, instr, (u32)val, false);

                break;
            }

            if(dst->kind == RD_OP_REG && src->kind == RD_OP_IMM) {
                _capstone_arm32_set_regval(ctx, dst->reg, (u32)src->s_imm);
            }
            else if(dst->kind == RD_OP_REG && src->kind == RD_OP_REG) {
                RDRegValue val;
                if(_capstone_arm32_get_regval(ctx, instr->address, src->reg,
                                              &val, capstone))
                    _capstone_arm32_set_regval(ctx, dst->reg, val);
                else
                    rd_del_regval_id(ctx, dst->reg);
            }
            else
                _capstone_arm32_clear_written(ctx, instr);

            break;
        }

        case ARM_INS_MOVW: {
            _capstone_arm32_set_regval(ctx, instr->operands[0].reg,
                                       (u32)instr->operands[1].s_imm & 0xFFFF);
            break;
        }

        case ARM_INS_MOVT: {
            RDRegValue lo;
            if(_capstone_arm32_get_regval(ctx, instr->address,
                                          instr->operands[0].reg, &lo,
                                          capstone)) {
                u32 result =
                    ((u32)instr->operands[1].s_imm << 16) | (lo & 0xFFFF);
                _capstone_arm32_set_regval(ctx, instr->operands[0].reg, result);
                if(rd_is_address(ctx, result))
                    rd_add_xref(ctx, instr->address, result, RD_DR_ADDRESS);
            }
            else
                rd_del_regval_id(ctx, instr->operands[0].reg);

            break;
        }

        case ARM_INS_LDR: {
            const RDOperand* dst = &instr->operands[0];
            const RDOperand* src = &instr->operands[1];
            if(dst->kind != RD_OP_REG) break;

            RDAddress addr = 0;
            bool known = false;

            if(src->kind == RD_OP_MEM) {
                addr = src->mem;
                known = true;
            }
            else if(src->kind == RD_OP_DISPL) {
                RDRegValue base;
                if(src->displ.index == RD_REGID_INVALID &&
                   _capstone_arm32_get_regval(
                       ctx, instr->address, src->displ.base, &base, capstone)) {

                    // post-index reads [base]
                    addr = (u32)base +
                           (src->userdata1 ? 0 : (i32)src->displ.offset);
                    known = true;
                }
            }

            u32 val;
            bool ok = known && _capstone_arm32_read_const32(ctx, addr, &val);

            if(dst->reg == ARM_REG_PC) {
                // never track pc in the register map, just follow the target
                if(ok) _capstone_arm32_branch_to(ctx, instr, val, false);
            }
            else if(ok)
                _capstone_arm32_set_regval(ctx, dst->reg, val);
            else
                rd_del_regval_id(ctx, dst->reg);

            if(src->kind == RD_OP_DISPL && instr->write_back)
                rd_del_regval_id(ctx, src->displ.base);

            break;
        }

        case ARM_INS_BX:
        case ARM_INS_BLX: {
            const RDOperand* op = &instr->operands[0];

            // blx imm is an address operand, see _capstone_arm32_xref_operands
            if(op->kind != RD_OP_REG) break;
            if(op->reg == ARM_REG_LR || op->reg == ARM_REG_PC) break;

            RDRegValue v;

            // unknown value: do nothing
            if(_capstone_arm32_get_regval(ctx, instr->address, op->reg, &v,
                                          capstone))
                _capstone_arm32_branch_to(ctx, instr, (u32)v, true);

            break;
        }

        default: _capstone_arm32_clear_written(ctx, instr); break;
    }
}

static void _capstone_arm32_xref_operands(RDContext* ctx,
                                          const RDInstruction* instr,
                                          bool is_thumb) {
    rd_foreach_operand(i, op, instr) {
        if(op->kind == RD_OP_ADDR) {
            if(rd_instr_is_branch(instr)) {
                // propagate THUMB status (blx imm switches it)
                bool target_thumb =
                    instr->id == ARM_INS_BLX ? !is_thumb : is_thumb;

                rd_auto_sregval(ctx, op->addr, "T", target_thumb);
                rd_add_xref(ctx, instr->address, op->addr,
                            _capstone_arm32_is_call(ctx, instr) ? RD_CR_CALL
                                                                : RD_CR_JUMP);
            }
            else // adr
                rd_add_xref(ctx, instr->address, op->addr, RD_DR_ADDRESS);
        }
        else if(op->kind == RD_OP_MEM && rd_is_address(ctx, op->mem)) {
            rd_add_xref(ctx, instr->address, op->mem, RD_DR_READ);

            // ldr rX, <literal>: the word is a pointer
            if(instr->id == ARM_INS_LDR && i == 1)
                rd_auto_type(ctx, op->mem, "u32", 0, RD_TYPE_PTR);
        }
    }
}

static bool _capstone_arm32_decode_alias(const cs_insn* cs_insn,
                                         RDInstruction* instr) {
    if(!cs_insn->is_alias) return false;

    const cs_arm* d = &cs_insn->detail->arm;

    switch(cs_insn->alias_id) {
        case ARM_INS_ALIAS_POP: {
            instr->id = ARM_INS_POP;

            if(capstone_arm32_decode_regmask(cs_insn, instr, 0))
                _capstone_arm32_set_return(d, instr);

            break;
        }

        case ARM_INS_ALIAS_PUSH: {
            instr->id = ARM_INS_PUSH;
            capstone_arm32_decode_regmask(cs_insn, instr, 0);
            break;
        }

        default: return false;
    }

    return true;
}

void capstone_plugin_arm32_emulate(RDContext* ctx, const RDInstruction* instr,
                                   RDProcessor* p) {
    RDRegValue t;
    bool is_thumb = rd_get_sregval(ctx, instr->address, "T", &t) && t != 0;
    bool is_call = _capstone_arm32_is_call(ctx, instr);

    _capstone_arm32_track(ctx, instr, p);
    _capstone_arm32_xref_operands(ctx, instr, is_thumb);

    if(is_call)
        _capstone_arm32_clobber(ctx, true);
    else if(instr->id == ARM_INS_SVC)
        _capstone_arm32_clobber(ctx, false);

    if(rd_instr_can_flow(instr) || is_call) {
        if(rd_instr_is_branch(instr))
            rd_auto_sregval(ctx, instr->address + instr->length, "T", is_thumb);

        rd_flow(ctx, instr->address + instr->length);
    }
}

bool capstone_plugin_arm32_render_operand(RDRenderer* r,
                                          const RDInstruction* instr, int idx,
                                          RDProcessor* p) {
    RD_UNUSED(p);

    RDContext* ctx = rd_renderer_get_context(r);
    const RDOperand* op = &instr->operands[idx];

    if(op->kind == RD_OP_MEM && instr->id == ARM_INS_LDR && idx == 1) {
        u32 dst;
        bool ok = _capstone_arm32_read_const32(ctx, op->mem, &dst);
        if(!ok) return false;

        RDAddress addr = dst;
        if((dst & 1) && rd_is_code(ctx, dst & ~1U)) addr = dst & ~1U;

        rd_renderer_norm(r, "=");

        if(rd_is_address(ctx, dst))
            rd_renderer_loc(r, addr, 8, RD_NUM_DEFAULT);
        else
            rd_renderer_num(r, addr, 16, 0, RD_NUM_DEFAULT);

        return true;
    }

    if(op->kind == RD_OP_DISPL) {
        rd_renderer_norm(r, "[");
        rd_renderer_reg(r, op->displ.base);

        if(!op->userdata1 && op->displ.offset != 0) {
            rd_renderer_norm(r, ", #");
            rd_renderer_num(r, op->displ.offset, 10, 0, RD_NUM_SIGNED);
        }

        rd_renderer_norm(r, "]");

        if(op->userdata1) { // post-index
            if(op->displ.offset != 0) {
                rd_renderer_norm(r, ", #");
                rd_renderer_num(r, op->displ.offset, 10, 0, RD_NUM_SIGNED);
            }
        }
        else if(instr->write_back)
            rd_renderer_norm(r, "!");

        return true;
    }

    if(op->kind == ARM_OP_REGMASK) {
        rd_renderer_norm(r, "{");
        bool first = true;

        for(int i = 0; i < 16; i++) {
            if(!(op->imm & (1U << i))) continue;
            if(!first) rd_renderer_norm(r, ", ");

            rd_renderer_reg(r, _capstone_arm32_index_to_reg(i));
            first = false;
        }

        rd_renderer_norm(r, "}");
        return true;
    }

    if(op->kind == ARM_OP_PSR) {
        char name[10]; // "cpsr_fsxc" + '\0'
        static_assert(sizeof("cpsr_fsxc") <= sizeof(name),
                      "PSR name buffer too small");

        usize n = 4;
        memcpy(name, op->user.val1 ? "spsr" : "cpsr", n);

        if(op->user.val2 && op->user.val2 <= 0xF) {
            name[n++] = '_';
            if(op->user.val2 & 8) name[n++] = 'f';
            if(op->user.val2 & 4) name[n++] = 's';
            if(op->user.val2 & 2) name[n++] = 'x';
            if(op->user.val2 & 1) name[n++] = 'c';
        }

        name[n] = '\0';
        rd_renderer_text(r, name, RD_THEME_REG, RD_THEME_BACKGROUND);
        return true;
    }

    return false;
}

bool capstone_arm32_decode_regmask(const cs_insn* cs_insn, RDInstruction* instr,
                                   usize idx) {
    const cs_arm* d = &cs_insn->detail->arm;
    RDOperand* op = &instr->operands[idx];
    op->kind = ARM_OP_REGMASK;
    op->imm = 0;

    for(uint8_t i = idx; i < d->op_count; i++) {
        if(d->operands[i].type == ARM_OP_REG) {
            int bit = _capstone_arm32_reg_to_index(d->operands[i].reg);
            if(bit >= 0) op->imm |= (1U << bit);
        }
    }

    return op->imm & (1U << 15);
}

bool capstone_arm32_simplify(const cs_insn* cs_insn, RDInstruction* instr,
                             RDProcessor* p) {
    const ARMCapstone* capstone = (const ARMCapstone*)p;

    const cs_arm* d = &cs_insn->detail->arm;

    switch(instr->id) {
        case ARM_INS_ADD:
        case ARM_INS_SUB: {
            // add/sub rd, pc, #imm  ->  adr rd, <address>
            if(d->op_count != 3 || d->update_flags) return false;
            if(d->cc != ARMCC_AL && d->cc != ARMCC_Invalid) return false;

            const cs_arm_op* rd = &d->operands[0];
            const cs_arm_op* rn = &d->operands[1];
            const cs_arm_op* imm = &d->operands[2];

            if(rd->type != ARM_OP_REG || rd->reg == ARM_REG_PC ||
               rn->type != ARM_OP_REG || rn->reg != ARM_REG_PC ||
               imm->type != ARM_OP_IMM)
                return false;

            i64 off = instr->id == ARM_INS_SUB ? -imm->imm : imm->imm;

            RDOperand* dst = rd_instr_set_op_reg(instr, 0, rd->reg);
            dst->userdata2 = (rd->access & CS_AC_WRITE) != 0;

            RDAddress pc = capstone->get_pc(instr->address);
            RDAddress target = pc + off;

            RDOperand* op = rd_instr_set_op_addr(instr, 1, target & ~1U);
            op->userdata1 = target & 1; // exact runtime value

            rd_instr_set_mnemonic(instr, "adr");
            return true;
        }

        default: return false;
    }
}

bool capstone_arm32_decode_flow(const cs_insn* cs_insn, RDInstruction* instr) {
    if(_capstone_arm32_decode_alias(cs_insn, instr)) return true;

    const cs_arm* d = &cs_insn->detail->arm;

    switch(instr->id) {
        case ARM_INS_UDF: instr->flow = RD_IF_STOP; break;

        case ARM_INS_BX: {
            if(d->operands[0].reg == ARM_REG_LR)
                _capstone_arm32_set_return(d, instr);
            else
                instr->flow = (d->cc == ARMCC_AL || d->cc == ARMCC_Invalid)
                                  ? RD_IF_JUMP
                                  : RD_IF_JUMP_COND;
            break;
        }

        case ARM_INS_CBZ:
        case ARM_INS_CBNZ: {
            instr->flow = RD_IF_JUMP_COND;
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

        case ARM_INS_STM:
        case ARM_INS_STMDA:
        case ARM_INS_STMDB:
        case ARM_INS_STMIB: {
            if(cs_insn->is_alias) {
                capstone_arm32_decode_regmask(cs_insn, instr, 0);
            }
            else {
                instr->operands[0].kind = RD_OP_REG;
                instr->operands[0].reg = d->operands[0].reg;
                capstone_arm32_decode_regmask(cs_insn, instr, 1);
            }

            return true;
        }

        case ARM_INS_LDM:
        case ARM_INS_LDMDA:
        case ARM_INS_LDMDB:
        case ARM_INS_LDMIB: {
            if(cs_insn->is_alias) {
                if(capstone_arm32_decode_regmask(cs_insn, instr, 0))
                    _capstone_arm32_set_return(d, instr);
            }
            else {
                instr->operands[0].kind = RD_OP_REG;
                instr->operands[0].reg = d->operands[0].reg;

                if(capstone_arm32_decode_regmask(cs_insn, instr, 1))
                    _capstone_arm32_set_return(d, instr);
            }

            return true;
        }

        case ARM_INS_PUSH:
            capstone_arm32_decode_regmask(cs_insn, instr, 0);
            return true;

        case ARM_INS_POP: {
            if(capstone_arm32_decode_regmask(cs_insn, instr, 0))
                _capstone_arm32_set_return(d, instr);
            return true;
        }

        case ARM_INS_LDR: {
            if(d->operands[0].reg == ARM_REG_PC) {
                const cs_arm_op* m = &d->operands[1];

                if((cs_insn->is_alias && d->post_index) ||
                   (m->type == ARM_OP_MEM && m->mem.base == ARM_REG_SP))
                    _capstone_arm32_set_return(
                        d, instr); // pop {pc}; ldr pc, [sp], #4
                else if(d->post_index) {
                    // ldr pc, [rN], #imm: walks a pointer, not a plain jump
                    break;
                }
                else {
                    instr->flow = (d->cc == ARMCC_AL || d->cc == ARMCC_Invalid)
                                      ? RD_IF_JUMP
                                      : RD_IF_JUMP_COND;
                }
            }

            break;
        }

        // data processing instructions writing to pc
        case ARM_INS_MOV:
        case ARM_INS_SUB:
        case ARM_INS_ADD: {
            if(d->op_count < 2 || d->operands[0].type != ARM_OP_REG ||
               d->operands[0].reg != ARM_REG_PC)
                break;

            const cs_arm_op* s = &d->operands[1];

            // mov pc, lr; subs pc, lr, #n; ...
            // anything else (mov pc, rN; add pc, pc, rX) flows: the target is
            // computed, so we don't pretend to know it
            if(s->type == ARM_OP_REG && s->reg == ARM_REG_LR)
                _capstone_arm32_set_return(d, instr);

            break;
        }

        default: break;
    }

    return false;
}

void capstone_arm32_decode_psr(const cs_arm_op* cop, RDOperand* op) {
    op->kind = ARM_OP_PSR;
    op->user.val1 = cop->type == ARM_OP_SPSR; // 0 = CPSR, 1 = SPSR
    op->user.val2 = cop->sysop.msr_mask;      // bit3=f, bit2=s, bit1=x, bit0=c
}
