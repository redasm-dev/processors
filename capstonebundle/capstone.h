#pragma once

#include <capstone/capstone.h>
#include <redasm/redasm.h>

typedef struct CapstoneInitData {
    cs_arch arch;
    cs_mode mode;
} CapstoneInitData;

typedef struct Capstone {
    const CapstoneInitData* data;
    csh handle;
    cs_insn* insn;
} Capstone;

RDProcessor* capstone_create(const RDProcessorPlugin* p);
void capstone_destroy(RDProcessor* p);
const char* capstone_get_reg_name(RDReg r, RDProcessor* p);
const cs_insn* capstone_decode(RDInstruction* instr, const char* code, usize n,
                               RDProcessor* p);
const char* capstone_get_mnemonic(const RDInstruction* instr, RDProcessor* p);
