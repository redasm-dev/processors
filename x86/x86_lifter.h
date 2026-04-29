#pragma once

#include <redasm/redasm.h>

void x86_lift(RDContext* ctx, const RDInstruction* instr, RDILInstruction* il,
              RDProcessor* p);
