#include "arm32.h"
#include "capstone.h"

static const CapstoneInitData ARM32_LE_INIT = {
    .arch = CS_ARCH_ARM,
    .mode = CS_MODE_ARM | CS_MODE_LITTLE_ENDIAN,
};

static const CapstoneInitData ARM32_BE_INIT = {
    .arch = CS_ARCH_ARM,
    .mode = CS_MODE_ARM | CS_MODE_BIG_ENDIAN,
};

static void _arm32_decode(RDContext* ctx, RDInstruction* instr,
                          RDProcessor* p) {}

static void _arm32_emulate(RDContext* ctx, const RDInstruction* instr,
                           RDProcessor* p) {}

static bool _arm32_render_operand(RDRenderer* r, const RDInstruction* instr,
                                  usize idx, RDProcessor* p) {
    return false;
}

const RDProcessorPlugin ARM32_LE = {
    .level = RD_API_LEVEL,
    .id = "arm32_le",
    .name = "ARM32 (Little Endian)",
    .flags = RD_PF_LE,
    .ptr_size = sizeof(u32),
    .int_size = sizeof(u32),
    .userdata = (void*)&ARM32_LE_INIT,
    .create = capstone_create,
    .destroy = capstone_destroy,
    .decode = _arm32_decode,
    .emulate = _arm32_emulate,
    .render_operand = _arm32_render_operand,
    .get_mnemonic = capstone_get_mnemonic,
    .get_reg_name = capstone_get_reg_name,
};

const RDProcessorPlugin ARM32_BE = {
    .level = RD_API_LEVEL,
    .id = "arm64_be",
    .name = "ARM32 (Big Endian)",
    .flags = RD_PF_BE,
    .ptr_size = sizeof(u32),
    .int_size = sizeof(u32),
    .userdata = (void*)&ARM32_BE_INIT,
    .create = capstone_create,
    .destroy = capstone_destroy,
    .decode = _arm32_decode,
    .emulate = _arm32_emulate,
    .render_operand = _arm32_render_operand,
    .get_mnemonic = capstone_get_mnemonic,
    .get_reg_name = capstone_get_reg_name,
};
