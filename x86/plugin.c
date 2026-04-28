#include "x86_common.h"
#include "x86_lifter.h"
#include <Zydis/Zydis.h>
#include <redasm/redasm.h>
#include <stdlib.h>

static const int SEGMENT_REGISTERS[6] = {
    ZYDIS_REGISTER_ES, ZYDIS_REGISTER_CS, ZYDIS_REGISTER_SS,
    ZYDIS_REGISTER_DS, ZYDIS_REGISTER_FS, ZYDIS_REGISTER_GS,
};

// clang-format off
static const char* x86_16_prologues[] = {
    "55 89 E5 83 EC ??", // push bp; mov bp, sp; sub sp, N (small frame)
    "55 89 E5",          // push bp; mov bp, sp
    "C8 ?? ?? 00",       // enter N, 0
    NULL,
};

static const char* x86_32_prologues[] = {
    "55 8B EC 81 EC ?? ?? ?? ??", // push ebp; mov ebp, esp; sub esp, NNNN (large frame)
    "55 8B EC 83 EC ??",          // push ebp; mov ebp, esp; sub esp, N (small frame)
    "8B FF 55 8B EC",             // mov edi, edi; push ebp; mov ebp, esp (ms-hotpatch)
    "55 8B EC",                   // push ebp; mov ebp, esp
    "C8 ?? ?? 00",                // enter N, 0
    NULL,
};
// clang-format on

// const RDCallingConvention* x86_32_calling_conventions[] = {
//     &x86_cc::cdecl_cc,
//     nullptr,
// };

typedef struct X86UserData {
    ZydisMachineMode mode;
    ZydisStackWidth width;
} X86UserData;

typedef struct X86Processor {
    ZydisDecoder decoder;
    char buffer[ZYDIS_MAX_INSTRUCTION_LENGTH];
    const char** prologues;
    // const RDCallingConvention** calling_conventions{nullptr};
} X86Processor;

static bool x86_is_segment_reg(const RDOperand* op) {
    if(op->kind != RD_OP_REG) return false;

    switch(op->reg) {
        case ZYDIS_REGISTER_ES:
        case ZYDIS_REGISTER_CS:
        case ZYDIS_REGISTER_SS:
        case ZYDIS_REGISTER_DS:
        case ZYDIS_REGISTER_FS:
        case ZYDIS_REGISTER_GS: return true;

        default: break;
    }

    return false;
}

static RDAddress x86_get_ip_value(const RDInstruction* instr) {
    return instr->address + instr->length;
}

static bool x86_is_addr_operand(const RDContext* ctx,
                                const RDInstruction* instr,
                                const ZydisDecodedOperand* zop) {
    if(zop->type != ZYDIS_OPERAND_TYPE_IMMEDIATE) return false;
    if(!rd_is_address(ctx, zop->imm.value.u)) return false;

    switch(instr->id) {
        case ZYDIS_MNEMONIC_PUSH:
        case ZYDIS_MNEMONIC_MOV: return true;
        default: break;
    }

    return false;
}

static void x86_try_set_type(RDContext* ctx, const RDOperand* op,
                             RDAddress address) {
    const RDSegment* seg = rd_find_segment(ctx, address);
    if(!seg || (seg->perm & RD_SP_X)) return;

    const char* t = NULL;

    switch(op->size) {
        case 8: t = "u8"; break;
        case 16: t = "u16"; break;
        case 32: t = "u32"; break;
        case 64: t = "u64"; break;
        default: return;
    }

    rd_auto_type(ctx, address, t, op->count, RD_TYPE_NONE);
}

// static RDRegValue x86_get_reg_val(RDEmulator* e, int reg) {
//     if(reg == ZYDIS_REGISTER_CS) {
//         const RDSegment* seg = rdemulator_getsegment(e);
//         if(seg) return RDRegValue_some(seg->start);
//     }
//
//     auto val = rdemulator_getreg(e, reg);
//
//     if(!val.ok && reg == ZYDIS_REGISTER_DS) {
//         const RDSegment* seg = rdemulator_getsegment(e);
//         if(seg) return RDRegValue_some(seg->start);
//     }
//
//     return val;
// }

bool x86_track_pop_reg(RDContext* ctx, const RDInstruction* instr) {
    //     RDInstruction previnstr;
    //     bool ok = rd_decode_prev(instr->address, &previnstr);
    //     RDRegValue val = RDRegValue_none();
    //
    //     if(ok && previnstr.id == ZYDIS_MNEMONIC_PUSH &&
    //        previnstr.operands[0].type == OP_REG) {
    //         val = get_reg_val(e, previnstr.operands[0].reg);
    //     }
    //     else
    //         val = get_reg_val(e, instr->operands[0].reg);
    //
    //     if(val.ok) {
    //         rd_setsreg(instr->address + instr->length,
    //         instr->operands[0].reg,
    //                    val.value);
    //     }
    //
    //     return val.ok;
    return false;
}

bool x86_track_mov_reg(RDContext* ctx, const RDInstruction* instr) {
    //     auto val = get_reg_val(e, instr->operands[0].reg);
    //
    //     if(val.ok) {
    //         rd_setsreg(instr->address + instr->length,
    //         instr->operands[0].reg,
    //                    val.value);
    //     }
    //
    return false;
}

bool x86_track_segment_reg(RDContext* ctx, const RDInstruction* instr) {
    if(instr->id == ZYDIS_MNEMONIC_POP &&
       x86_is_segment_reg(&instr->operands[0]))
        return x86_track_pop_reg(ctx, instr);

    if(instr->id == ZYDIS_MNEMONIC_MOV &&
       x86_is_segment_reg(&instr->operands[0]))
        return x86_track_mov_reg(ctx, instr);

    return false;
}

static void x86_decode(const RDContext* ctx, RDInstruction* instr,
                       RDProcessor* proc) {
    X86Processor* self = (X86Processor*)proc;
    usize n = rd_read(ctx, instr->address, self->buffer, sizeof(self->buffer));

    ZydisDecodedInstruction zinstr;
    ZydisDecodedOperand zops[ZYDIS_MAX_OPERAND_COUNT];

    if(!n || !ZYAN_SUCCESS(ZydisDecoderDecodeFull(&self->decoder, self->buffer,
                                                  n, &zinstr, zops))) {
        return;
    }

    instr->id = zinstr.mnemonic;
    instr->length = zinstr.length;

    switch(zinstr.meta.category) {
        case ZYDIS_CATEGORY_CALL: instr->flow = RD_IF_CALL; break;
        case ZYDIS_CATEGORY_NOP: instr->flow = RD_IF_NOP; break;
        case ZYDIS_CATEGORY_COND_BR: instr->flow = RD_IF_JUMP_COND; break;
        case ZYDIS_CATEGORY_UNCOND_BR: instr->flow = RD_IF_JUMP; break;
        default: break;
    };

    for(int i = 0, j = 0; i < zinstr.operand_count; i++) {
        const ZydisDecodedOperand* zop = &zops[i];
        if(zop->visibility == ZYDIS_OPERAND_VISIBILITY_HIDDEN) continue;

        RDOperand* op = &instr->operands[j++];
        op->size = zop->element_size;
        op->count = zop->element_count > 1 ? zop->element_count : 0;

        switch(zop->type) {
            case ZYDIS_OPERAND_TYPE_REGISTER:
                op->kind = RD_OP_REG;
                op->reg = zop->reg.value;
                break;

            case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
                ZyanU64 addr = 0;

                if(rd_is_branch(instr) &&
                   ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
                       &zinstr, zop, instr->address, &addr))) {
                    op->kind = RD_OP_ADDR;
                    op->addr = addr;
                }
                else if(x86_is_addr_operand(ctx, instr, zop)) {
                    op->kind = RD_OP_ADDR;
                    op->addr = zop->imm.value.u;
                }
                else {
                    op->kind = RD_OP_IMM;
                    op->imm = zop->imm.value.u;
                }
                break;
            }

            case ZYDIS_OPERAND_TYPE_MEMORY: {
                if(zop->mem.base != ZYDIS_REGISTER_NONE &&
                   zop->mem.index != ZYDIS_REGISTER_NONE &&
                   !zop->mem.disp.has_displacement) {
                    op->kind = RD_OP_PHRASE;
                    op->phrase.base = zop->mem.base;
                    op->phrase.index = zop->mem.index;
                    op->userdata1 = zop->mem.segment;
                }
                else if(zop->mem.base == ZYDIS_REGISTER_NONE &&
                        zop->mem.index == ZYDIS_REGISTER_NONE) {
                    op->kind = RD_OP_MEM;

                    ZyanU64 addr;

                    if(rd_is_branch(instr) &&
                       ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
                           &zinstr, zop, instr->address, &addr))) {
                        op->mem = addr;
                    }
                    else
                        op->mem = zop->mem.disp.value;

                    op->userdata1 = zop->mem.segment;
                }
                else if(zop->mem.base == x86_get_ip(ctx) &&
                        zop->mem.index == ZYDIS_REGISTER_NONE) {
                    op->kind = RD_OP_MEM;
                    op->mem = x86_get_ip_value(instr) + zop->mem.disp.value;
                }
                else {
                    op->kind = RD_OP_DISPL;
                    op->displ.base = zop->mem.base;
                    op->displ.index = zop->mem.index;
                    op->displ.scale = zop->mem.scale;
                    op->displ.displ = zop->mem.disp.value;
                    op->userdata1 = zop->mem.segment;
                }
                break;
            }

            default: break;
        }
    }

    switch(zinstr.mnemonic) {
        case ZYDIS_MNEMONIC_HLT:
        case ZYDIS_MNEMONIC_INT3:
        case ZYDIS_MNEMONIC_RET:
        case ZYDIS_MNEMONIC_IRET: instr->flow = RD_IF_STOP; break;

        case ZYDIS_MNEMONIC_INT:
            // rd_getenvironment()->update_instruction(instr);
            break;

        default: break;
    }
}

static void x86_render_instruction(RDRenderer* r, const RDInstruction* instr,
                                   RDProcessor* proc) {
    RD_UNUSED(proc);

    switch(instr->flow) {
        case RD_IF_JUMP: rd_renderer_mnem(r, instr, RD_THEME_JUMP); break;

        case RD_IF_JUMP_COND:
            rd_renderer_mnem(r, instr, RD_THEME_JUMP_COND);
            break;

        case RD_IF_CALL: rd_renderer_mnem(r, instr, RD_THEME_CALL); break;

        case RD_IF_CALL_COND:
            rd_renderer_mnem(r, instr, RD_THEME_CALL_COND);
            break;

        case RD_IF_NOP: rd_renderer_mnem(r, instr, RD_THEME_MUTED); break;
        case RD_IF_STOP: rd_renderer_mnem(r, instr, RD_THEME_STOP); break;
        default: rd_renderer_mnem(r, instr, RD_THEME_FOREGROUND); break;
    }

    rd_foreach_operand(i, op, instr) {
        if(i > 0) rd_renderer_norm(r, ", ");

        switch(op->kind) {
            case RD_OP_ADDR: rd_renderer_loc(r, op->addr, 0, 0); break;

            case RD_OP_IMM:
                rd_renderer_cnst(r, op->imm, 16, 0, RD_NUM_DEFAULT);
                break;

            case RD_OP_MEM: {
                rd_renderer_norm(r, "[");

                if(op->userdata1 && op->userdata1 != ZYDIS_REGISTER_CS &&
                   op->userdata1 != ZYDIS_REGISTER_DS) {
                    rd_renderer_reg(r, op->userdata1);
                    rd_renderer_norm(r, ":");
                }

                rd_renderer_loc(r, op->mem, 0, 0);
                rd_renderer_norm(r, "]");
                break;
            }

            case RD_OP_REG: rd_renderer_reg(r, op->reg); break;

            case RD_OP_PHRASE: {
                rd_renderer_norm(r, "[");
                rd_renderer_reg(r, op->phrase.base);
                rd_renderer_norm(r, "+");
                rd_renderer_reg(r, op->phrase.index);
                rd_renderer_norm(r, "]");
                break;
            }

            case RD_OP_DISPL: {
                rd_renderer_norm(r, "[");
                rd_renderer_reg(r, op->displ.base);

                if(op->displ.index != ZYDIS_REGISTER_NONE) {
                    rd_renderer_norm(r, "+");
                    rd_renderer_reg(r, op->displ.index);

                    if(op->displ.scale > 1) {
                        rd_renderer_norm(r, "*");
                        rd_renderer_cnst(r, op->displ.scale, 16, 0,
                                         RD_NUM_DEFAULT);
                    }
                }

                if(op->displ.displ != 0)
                    rd_renderer_loc(r, op->displ.displ, 0, RD_NUM_SIGNED);

                rd_renderer_norm(r, "]");
                break;
            }

            default: break;
        }
    }
}

static void x86_emulate(RDContext* ctx, const RDInstruction* instr,
                        RDProcessor* proc) {
    RD_UNUSED(proc);

    if(!x86_track_segment_reg(ctx, instr)) {
        rd_foreach_operand(i, op, instr) {
            switch(op->kind) {
                case RD_OP_ADDR: {
                    if(rd_is_jump(instr))
                        rd_add_xref(ctx, instr->address, op->addr, RD_CR_JUMP);
                    else if(rd_is_call(instr))
                        rd_add_xref(ctx, instr->address, op->addr, RD_CR_CALL);
                    else {
                        rd_add_xref(ctx, instr->address, op->addr,
                                    RD_DR_ADDRESS);
                    }

                    break;
                }

                case RD_OP_MEM: {
                    if(rd_is_jump(instr)) {
                        X86Address addr = x86_read_address(ctx, op->mem);
                        if(addr.has_value) {
                            rd_add_xref(ctx, instr->address, addr.value,
                                        RD_CR_JUMP);
                        }
                    }
                    else if(rd_is_call(instr)) {
                        X86Address addr = x86_read_address(ctx, op->mem);
                        if(addr.has_value) {
                            rd_add_xref(ctx, instr->address, addr.value,
                                        RD_CR_CALL);
                        }
                    }
                    else
                        x86_try_set_type(ctx, op, op->mem);

                    rd_add_xref(ctx, instr->address, op->mem, RD_DR_ADDRESS);
                    break;
                }

                default: break;
            }
        }
    }

    if(rd_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

static RDProcessor* x86_processor_create(const RDProcessorPlugin* plugin) {
    X86UserData* ud = (X86UserData*)plugin->userdata;
    X86Processor* self = calloc(1, sizeof(X86Processor));

    if(ud->mode == ZYDIS_MACHINE_MODE_LEGACY_32)
        self->prologues = x86_32_prologues;
    else if(ud->mode == ZYDIS_MACHINE_MODE_LEGACY_16)
        self->prologues = x86_16_prologues;
    else
        self->prologues = NULL;

    ZydisDecoderInit(&self->decoder, ud->mode, ud->width);
    return (RDProcessor*)self;
}

static void x86_processor_destroy(RDProcessor* p) { free(p); }

static const char* x86_processor_get_mnemonic(const RDInstruction* instr,
                                              RDProcessor* p) {
    RD_UNUSED(p);
    return ZydisMnemonicGetString((ZydisMnemonic)instr->id);
}

static const char* x86_processor_get_reg(int reg, RDProcessor* p) {
    RD_UNUSED(p);
    return ZydisRegisterGetString((ZydisRegister)reg);
}

static const char** x86_processor_get_prologues(RDProcessor* p,
                                                RDContext* ctx) {
    RD_UNUSED(ctx);
    X86Processor* self = (X86Processor*)p;
    return self->prologues;
}

static void x86_register_processor(RDProcessorPlugin* plugin,
                                   ZydisMachineMode mode, ZydisStackWidth width,
                                   const char* id, const char* name,
                                   int addrsize, int intsize) {
    X86UserData* ud = malloc(sizeof(*ud));
    ud->mode = mode;
    ud->width = width;

    plugin->level = RD_API_LEVEL;
    plugin->id = id;
    plugin->name = name;
    plugin->ptr_size = addrsize;
    plugin->int_size = intsize;
    plugin->userdata = ud;
    plugin->decode = x86_decode;
    plugin->emulate = x86_emulate;
    plugin->lift = x86_lift;
    plugin->render_instruction = x86_render_instruction;
    plugin->create = x86_processor_create;
    plugin->destroy = x86_processor_destroy;
    plugin->get_mnemonic = x86_processor_get_mnemonic;
    plugin->get_register = x86_processor_get_reg;
    plugin->get_prologues = x86_processor_get_prologues;

    // plugin->get_callingconventions = [](const RDProcessor* self) {
    //     return reinterpret_cast<const
    //     X86Processor*>(self)->calling_conventions;
    // };

    rd_register_processor(plugin);
}

static RDProcessorPlugin x86_16_real = {0};
static RDProcessorPlugin x86_16 = {0};
static RDProcessorPlugin x86_32 = {0};
static RDProcessorPlugin x86_64 = {0};

void rd_plugin_create(void) {
    x86_register_processor(&x86_16_real, ZYDIS_MACHINE_MODE_REAL_16,
                           ZYDIS_STACK_WIDTH_16, "x86_16_real",
                           "X86_16 (Real Mode)", sizeof(u16), sizeof(u16));

    x86_register_processor(&x86_16, ZYDIS_MACHINE_MODE_LEGACY_16,
                           ZYDIS_STACK_WIDTH_16, "x86_16", "X86_16",
                           sizeof(u16), sizeof(u16));

    x86_register_processor(&x86_32, ZYDIS_MACHINE_MODE_LEGACY_32,
                           ZYDIS_STACK_WIDTH_32, "x86_32", "X86_32",
                           sizeof(u32), sizeof(u32));

    x86_register_processor(&x86_64, ZYDIS_MACHINE_MODE_LONG_64,
                           ZYDIS_STACK_WIDTH_64, "x86_64", "X86_64",
                           sizeof(u64), sizeof(u64));
}

void rd_plugin_destroy(void) {
    free(x86_16_real.userdata);
    free(x86_16.userdata);
    free(x86_32.userdata);
    free(x86_64.userdata);
}
