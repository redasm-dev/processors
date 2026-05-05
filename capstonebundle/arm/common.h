#pragma once

#include <redasm/redasm.h>

void capstone_arm32_emulate(RDContext* ctx, const RDInstruction* instr,
                            RDProcessor* p);

bool capstone_arm32_render_operand(RDRenderer* r, const RDInstruction* instr,
                                   usize idx, RDProcessor* p);
