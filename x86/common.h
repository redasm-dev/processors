#pragma once

#include <Zydis/Zydis.h>
#include <redasm/redasm.h>

typedef struct X86Address {
    RDAddress value;
    bool has_value;
} X86Address;

X86Address x86_read_address(const RDContext* ctx, RDAddress address);

ZydisRegister x86_get_ip(const RDContext* ctx);
ZydisRegister x86_get_sp(const RDContext* ctx);
ZydisRegister x86_get_bp(const RDContext* ctx);
