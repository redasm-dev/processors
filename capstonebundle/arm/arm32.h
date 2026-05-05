#pragma once

#include <redasm/redasm.h>

extern const RDProcessorPlugin ARM32_LE;
extern const RDProcessorPlugin ARM32_BE;

void capstone_arm32_decode(RDContext* ctx, RDInstruction* instr,
                           RDProcessor* p);
