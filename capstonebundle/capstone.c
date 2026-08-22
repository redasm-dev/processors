#include "capstone.h"

Capstone* capstone_create(const CapstoneInitData* data, int size) {
    csh h;
    cs_err err = cs_open(data->arch, data->mode, &h);

    if(err) {
        RD_LOG_FAIL("%s", cs_strerror(err));
        return NULL;
    }

    Capstone* self = rd_alloc(size);
    *self = (Capstone){.data = data, .handle = h};

    cs_option(self->handle, CS_OPT_DETAIL, CS_OPT_ON);
    cs_option(self->handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_CS_REG_ALIAS);
    self->insn = cs_malloc(self->handle);
    return self;
}

void capstone_destroy(Capstone* self) {
    if(!self) return;

    if(self->handle) cs_close(&self->handle);
    if(self->insn) cs_free(self->insn, 1);
    rd_free(self);
}

RDProcessor* capstone_plugin_create(const RDProcessorPlugin* p) {
    const CapstoneInitData* data = (const CapstoneInitData*)p->userdata;
    return (RDProcessor*)capstone_create(data, sizeof(Capstone));
}

void capstone_plugin_destroy(RDProcessor* p) { capstone_destroy((Capstone*)p); }

bool capstone_plugin_query_reg(RDQueryReg* q, RDProcessor* p) {
    Capstone* self = (Capstone*)p;

    if(q->kind == RD_QUERY_REG_BY_ID) {
        q->name = cs_reg_name(self->handle, (unsigned int)q->id);
        if(!q->name) return false;
    }
    else // other flags not implemented
        return false;

    if(q->want & RD_QUERY_REG_WANT_CANONICAL) q->canonical_name = q->name;

    return true;
}

const cs_insn* capstone_plugin_decode(RDInstruction* instr, const char* code,
                                      usize n, RDProcessor* p) {
    Capstone* self = (Capstone*)p;

    const uint8_t** ptr = (const uint8_t**)&code;
    size_t len = (size_t)n;
    uint64_t addr = (uint64_t)instr->address;

    if(!cs_disasm_iter(self->handle, ptr, &len, &addr, self->insn)) return NULL;

    instr->id = self->insn->id;
    instr->length = self->insn->size;
    rd_instr_set_mnemonic(instr, self->insn->mnemonic);
    return self->insn;
}

const char* capstone_plugin_get_mnemonic(const RDInstruction* instr,
                                         RDProcessor* p) {
    RD_UNUSED(p);
    return instr->mnemonic_buf;
}
