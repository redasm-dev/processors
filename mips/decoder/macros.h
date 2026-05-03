#pragma once

#include "decoder/formats.h"
#include <redasm/redasm.h>

void mips_simplify(const RDContext* ctx, MIPSDecodedInstruction* dec,
                   const RDInstruction* instr);

void mips_decode_macro(const MIPSDecodedInstruction* dec, RDInstruction* instr);
