#include "capstone.h"
#include <stdlib.h>

static Capstone* _capstone_create(const CapstoneInitData* data) {
    csh h;
    cs_err err = cs_open(data->arch, data->mode, &h);

    if(err) {
        rd_log(RD_LOG_FAIL, "%s", cs_strerror(err));
        return NULL;
    }

    Capstone* self = malloc(sizeof(*self));

    *self = (Capstone){
        .data = data,
        .handle = h,
    };

    cs_option(self->handle, CS_OPT_DETAIL, CS_OPT_ON);
    self->insn = cs_malloc(self->handle);
    return self;
}

static void _capstone_destroy(Capstone* self) {
    if(!self) return;

    if(self->handle) cs_close(&self->handle);
    if(self->insn) cs_free(self->insn, 1);
    free(self);
}

RDProcessor* capstone_create(const RDProcessorPlugin* p) {
    const CapstoneInitData* data = (const CapstoneInitData*)p->userdata;
    return (RDProcessor*)_capstone_create(data);
}

void capstone_destroy(RDProcessor* p) { _capstone_destroy((Capstone*)p); }

const char* capstone_get_reg_name(RDReg r, RDProcessor* p) {
    Capstone* self = (Capstone*)p;
    return cs_reg_name(self->handle, (unsigned int)r);
}

const cs_insn* capstone_decode(RDInstruction* instr, const char* code, usize n,
                               RDProcessor* p) {
    Capstone* self = (Capstone*)p;

    const uint8_t** ptr = (const uint8_t**)&code;
    size_t len = (size_t)n;
    uint64_t addr = (uint64_t)instr->address;

    if(!cs_disasm_iter(self->handle, ptr, &len, &addr, self->insn)) return NULL;

    instr->id = self->insn->id;
    instr->length = self->insn->size;
    rd_instruction_set_mnemonic(instr, self->insn->mnemonic);
    return self->insn;
}

const char* capstone_get_mnemonic(const RDInstruction* instr, RDProcessor* p) {
    RD_UNUSED(p);
    return instr->mnemonic;
}
