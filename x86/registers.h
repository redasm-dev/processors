#pragma once

#include <redasm/redasm.h>

bool x86_is_segment_reg(RDReg r);
void x86_set_regval(RDContext* ctx, RDAddress address, RDReg id, RDRegValue v);
bool x86_get_regval(RDContext* ctx, RDAddress address, RDReg id, RDRegValue* v);
void x86_del_regval(RDContext* ctx, RDAddress address, RDReg id);
RDAddress x86_get_ip_value(const RDInstruction* instr);
void x86_track_math(RDContext* ctx, const RDInstruction* instr);
void x86_track_mov(RDContext* ctx, const RDInstruction* instr);
bool x86_track_pop(RDContext* ctx, const RDInstruction* instr);
bool x86_query_reg(RDQueryReg* q, RDProcessor* p);
