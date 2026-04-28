#pragma once

#include <redasm/redasm.h>

void x86_lift(const RDContext* ctx, const RDInstruction* instr,
              RDILInstruction* il, RDProcessor* p);
