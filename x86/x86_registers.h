#pragma once

#include <redasm/redasm.h>

void x86_snapshot_regs(RDContext* ctx, const RDInstruction* instr,
                       RDAddress target);
bool x86_is_segment_reg(const RDOperand* op);
RDAddress x86_get_ip_value(const RDInstruction* instr);
void x86_track_math(RDContext* ctx, const RDInstruction* instr);
void x86_track_mov(RDContext* ctx, const RDInstruction* instr);
bool x86_track_pop(RDContext* ctx, const RDInstruction* instr);
bool x86_get_reg_mask(const char* name, RDRegMask* m, RDProcessor* p);
