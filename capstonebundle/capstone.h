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

RDProcessor* capstone_plugin_create(const RDProcessorPlugin* p);
void capstone_plugin_destroy(RDProcessor* p);
const char* capstone_plugin_get_reg_name(RDReg r, RDProcessor* p);
const cs_insn* capstone_plugin_decode(RDInstruction* instr, const char* code,
                                      usize n, RDProcessor* p);
const char* capstone_plugin_get_mnemonic(const RDInstruction* instr,
                                         RDProcessor* p);

Capstone* capstone_create(const CapstoneInitData* data, int size);
void capstone_destroy(Capstone* self);
