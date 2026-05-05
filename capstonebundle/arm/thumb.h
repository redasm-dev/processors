#pragma once

#include "capstone.h"
#include <redasm/redasm.h>

extern const CapstoneInitData THUMB_LE_INIT;
extern const CapstoneInitData THUMB_BE_INIT;

extern const RDProcessorPlugin THUMB_LE;
extern const RDProcessorPlugin THUMB_BE;

void capstone_thumb_decode(RDContext* ctx, RDInstruction* instr,
                           RDProcessor* p);
