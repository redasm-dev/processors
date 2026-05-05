#pragma once

#include <capstone.h>
#include <redasm/redasm.h>

void capstone_plugin_arm32_emulate(RDContext* ctx, const RDInstruction* instr,
                                   RDProcessor* p);

bool capstone_plugin_arm32_render_operand(RDRenderer* r,
                                          const RDInstruction* instr, usize idx,
                                          RDProcessor* p);

bool capstone_arm32_decode_regmask(const cs_insn* cs_insn, RDInstruction* instr,
                                   usize idx);

bool capstone_arm32_decode_flow(const cs_insn* cs_insn, RDInstruction* instr);
