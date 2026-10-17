#pragma once

#include <capstone.h>
#include <redasm/redasm.h>

typedef RDAddress (*CapstoneArmGetPc)(RDAddress);

typedef struct ARMCapstone {
    Capstone base;
    CapstoneArmGetPc get_pc;
} ARMCapstone;

void capstone_plugin_arm32_emulate(RDContext* ctx, const RDInstruction* instr,
                                   RDProcessor* p);

bool capstone_plugin_arm32_render_operand(RDRenderer* r,
                                          const RDInstruction* instr, int idx,
                                          RDProcessor* p);

bool capstone_arm32_decode_regmask(const cs_insn* cs_insn, RDInstruction* instr,
                                   usize idx);

bool capstone_arm32_simplify(const cs_insn* cs_insn, RDInstruction* instr,
                             RDProcessor* p);
bool capstone_arm32_decode_flow(const cs_insn* cs_insn, RDInstruction* instr);
void capstone_arm32_decode_psr(const cs_arm_op* cop, RDOperand* op);
