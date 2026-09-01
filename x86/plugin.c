#include "common.h"
#include "encoder/encoder.h"
#include "lifter.h"
#include "registers.h"
#include <redasm/redasm.h>

/* Segment register arithmetic for 16-bit modes.
 *
 * Real mode (x86_16_real):
 *   seg holds a paragraph (physical_address / 16), so seg * 16 = base.
 *
 * Protected mode (x86_16):
 *   Selectors have no meaningful arithmetic value at runtime, but loaders
 *   maps segment N at N * 0x10000 and seeds segment registers as
 *   paragraphs: cs = (N * 0x10000) >> 4 = N * 0x1000.
 *   So seg * 16 = N * 0x1000 * 16 = N * 0x10000 is the correct flat base.
 *
 * The formula is identical for both modes.
 *
 * ANY LOADER TARGETING X86_16 MUST ALIGN SEGMENTS TO 0x10000 BOUNDARIES
 * AND SEED SEGMENT REGISTERS AS PARAGRAPHS FOR THIS TO HOLD.
 *
 * TLDR:
 * - align all segments to 0x10000
 * - segment registers keeps the index to mapped segments
 */

static RDAddress _x86_ptr_to_address(u16 seg_idx, u16 seg_offset,
                                     const RDContext* ctx) {
    const RDProcessorPlugin* plugin = rd_get_processor_plugin(ctx);
    const X86UserData* ud = (const X86UserData*)plugin->userdata;

    if(ud->mode == ZYDIS_MACHINE_MODE_REAL_16)
        return ((RDAddress)seg_idx * 0x10) + seg_offset;

    return ((RDAddress)seg_idx * 0x10000) + seg_offset;
}

static bool _x86_calc_address(RDAddress addr,
                              const ZydisDecodedInstruction* zinstr,
                              const ZydisDecodedOperand* zop, RDContext* ctx,
                              RDAddress* outaddr) {
    ZyanU64 zaddr = 0;
    if(!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(zinstr, zop, addr, &zaddr)))
        return false;

    const RDProcessorPlugin* plugin = rd_get_processor_plugin(ctx);
    const X86UserData* ud = (const X86UserData*)plugin->userdata;

    if(ud->mode == ZYDIS_MACHINE_MODE_REAL_16 ||
       ud->mode == ZYDIS_MACHINE_MODE_LEGACY_16) {
        RDRegValue seg_base = 0;

        if(zop->type == ZYDIS_OPERAND_TYPE_MEMORY)
            rd_get_sregval_id(ctx, addr, zop->mem.segment, &seg_base);
        else
            rd_get_sregval(ctx, addr, "cs", &seg_base);

        zaddr = ((RDAddress)seg_base * 16) + (zaddr & 0xFFFF);
    }

    *outaddr = (RDAddress)zaddr;
    return true;
}

static bool _x86_is_addr_operand(const RDContext* ctx,
                                 const RDInstruction* instr,
                                 const ZydisDecodedOperand* zop) {
    // 16-bit mode: never promote immediates to addresses.
    // Both real-mode and protected-mode 16-bit have the same problem:
    // - real-mode: segment:offset, base at 0, everything is ambiguous
    // - protected-mode: selectors are not addresses at all
    if(rd_get_processor_plugin(ctx)->ptr_size == sizeof(u16)) return false;

    if(zop->type != ZYDIS_OPERAND_TYPE_IMMEDIATE) return false;
    if(!rd_is_address(ctx, zop->imm.value.u)) return false;

    switch(instr->id) {
        case ZYDIS_MNEMONIC_PUSH:
        case ZYDIS_MNEMONIC_MOV: return true;
        default: break;
    }

    return false;
}

static void _x86_try_set_type(RDContext* ctx, const RDOperand* op,
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

static void _x86_default_emulate(RDContext* ctx, const RDInstruction* instr) {
    rd_foreach_operand(i, op, instr) {
        switch(op->kind) {
            case RD_OP_ADDR: {
                if(rd_instr_is_jump(instr))
                    rd_add_xref(ctx, instr->address, op->addr, RD_CR_JUMP);
                else if(rd_instr_is_call(instr))
                    rd_add_xref(ctx, instr->address, op->addr, RD_CR_CALL);
                else
                    rd_add_xref(ctx, instr->address, op->addr, RD_DR_ADDRESS);

                break;
            }

            case RD_OP_MEM: {
                if(rd_instr_is_jump(instr)) {
                    X86Address addr = x86_read_address(ctx, op->mem);
                    if(addr.has_value) {
                        rd_add_xref(ctx, instr->address, addr.value,
                                    RD_CR_JUMP);
                    }
                }
                else if(rd_instr_is_call(instr)) {
                    X86Address addr = x86_read_address(ctx, op->mem);
                    if(addr.has_value) {
                        rd_add_xref(ctx, instr->address, addr.value,
                                    RD_CR_CALL);
                    }
                }
                else
                    _x86_try_set_type(ctx, op, op->mem);

                rd_add_xref(ctx, instr->address, op->mem, RD_DR_READ);
                break;
            }

            default: break;
        }
    }
}

static void x86_decode(RDContext* ctx, RDInstruction* instr,
                       RDProcessor* proc) {
    X86Processor* self = (X86Processor*)proc;
    usize n = rd_read(ctx, instr->address, self->buffer, sizeof(self->buffer));

    ZydisDecodedInstruction zinstr = {0};
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
                if(rd_instr_is_branch(instr)) instr->indirect = true;
                break;

            case ZYDIS_OPERAND_TYPE_IMMEDIATE: {
                RDAddress addr = 0;

                if(rd_instr_is_branch(instr) &&
                   _x86_calc_address(instr->address, &zinstr, zop, ctx,
                                     &addr)) {
                    op->kind = RD_OP_ADDR;
                    op->addr = addr;
                }
                else if(_x86_is_addr_operand(ctx, instr, zop)) {
                    op->kind = RD_OP_ADDR;
                    op->addr = zop->imm.value.u;
                }
                else if(instr->id == ZYDIS_MNEMONIC_INT) {
                    op->kind = RD_OP_CNST;
                    op->cnst = zop->imm.value.u;
                }
                else {
                    op->kind = RD_OP_IMM;
                    op->imm = zop->imm.value.u;
                }
                break;
            }

            case ZYDIS_OPERAND_TYPE_MEMORY: {
                if(zop->mem.base == ZYDIS_REGISTER_NONE &&
                   zop->mem.index == ZYDIS_REGISTER_NONE) {
                    op->kind = RD_OP_MEM;

                    RDAddress addr = 0;

                    if(_x86_calc_address(instr->address, &zinstr, zop, ctx,
                                         &addr)) {
                        op->mem = addr;
                    }
                    else if(zop->mem.disp.has_displacement)
                        op->mem = (RDAddress)zop->mem.disp.value;

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
                    op->displ.index = zop->mem.index == ZYDIS_REGISTER_NONE
                                          ? RD_REGID_INVALID
                                          : zop->mem.index;
                    op->displ.scale = zop->mem.scale;

                    op->displ.offset = zop->mem.disp.has_displacement
                                           ? zop->mem.disp.value
                                           : 0;

                    op->userdata1 = zop->mem.segment;
                }

                if(rd_instr_is_branch(instr)) instr->indirect = true;
                break;
            }

            case ZYDIS_OPERAND_TYPE_POINTER: {
                op->kind = RD_OP_ADDR;
                op->addr =
                    _x86_ptr_to_address(zop->ptr.segment, zop->ptr.offset, ctx);
                break;
            }

            default: break;
        }
    }

    switch(zinstr.mnemonic) {
        case ZYDIS_MNEMONIC_HLT:
        case ZYDIS_MNEMONIC_INT3:
            instr->no_ret = true;
            instr->flow = RD_IF_STOP;
            break;

        case ZYDIS_MNEMONIC_RET:
        case ZYDIS_MNEMONIC_IRET: instr->flow = RD_IF_STOP; break;

        case ZYDIS_MNEMONIC_INT:
        case ZYDIS_MNEMONIC_INTO:
            rd_fire_decode_hook(ctx, "x86.int", instr);
            break;

        default: break;
    }
}

static bool x86_render_operand(RDRenderer* r, const RDInstruction* instr,
                               int idx, RDProcessor* proc) {
    RD_UNUSED(proc);
    const RDOperand* op = &instr->operands[idx];

    switch(op->kind) {
        case RD_OP_MEM: {
            rd_renderer_norm(r, "[");

            if(op->userdata1 && op->userdata1 != ZYDIS_REGISTER_CS &&
               op->userdata1 != ZYDIS_REGISTER_DS) {
                rd_renderer_reg(r, op->userdata1);
                rd_renderer_norm(r, ":");
            }

            rd_renderer_loc(r, op->mem, 0, 0);
            rd_renderer_norm(r, "]");
            return true;
        }

        default: break;
    }

    return false;
}

static void x86_emulate(RDContext* ctx, const RDInstruction* instr,
                        RDProcessor* proc) {
    RD_UNUSED(proc);

    bool fallback = true;

    switch(instr->id) {
        case ZYDIS_MNEMONIC_MOV: x86_track_mov(ctx, instr); break;

        case ZYDIS_MNEMONIC_POP: {
            if(instr->id == ZYDIS_MNEMONIC_POP)
                fallback = !x86_track_pop(ctx, instr);

            break;
        }

        case ZYDIS_MNEMONIC_ADD:
        case ZYDIS_MNEMONIC_SUB:
        case ZYDIS_MNEMONIC_AND:
        case ZYDIS_MNEMONIC_OR:
        case ZYDIS_MNEMONIC_XOR:
        case ZYDIS_MNEMONIC_SHL:
        case ZYDIS_MNEMONIC_SHR:
        case ZYDIS_MNEMONIC_SAR:
        case ZYDIS_MNEMONIC_INC:
        case ZYDIS_MNEMONIC_DEC:
        case ZYDIS_MNEMONIC_NEG:
        case ZYDIS_MNEMONIC_NOT: x86_track_math(ctx, instr); break;

        case ZYDIS_MNEMONIC_INT:
        case ZYDIS_MNEMONIC_INTO:
            rd_fire_emulate_hook(ctx, "x86.int", instr);
            break;

        default: break;
    }

    if(fallback) _x86_default_emulate(ctx, instr);
    if(rd_instr_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

static RDProcessor* x86_create(const RDProcessorPlugin* plugin) {
    const X86UserData* ud = (X86UserData*)plugin->userdata;
    X86Processor* self = rd_alloc0(1, sizeof(X86Processor));
    self->userdata = ud;
    self->lex = rd_lexer_create(NULL);
    rd_lexer_set_default_base(self->lex, 16);

    ZydisDecoderInit(&self->decoder, ud->mode, ud->width);
    return (RDProcessor*)self;
}

static void x86_destroy(RDProcessor* p) {
    X86Processor* self = (X86Processor*)p;
    rd_lexer_destroy(self->lex);
    rd_free(self);
}

static void x86_setup(RDContext* ctx, RDProcessor* p) {
    X86Processor* self = (X86Processor*)p;

    switch(self->userdata->mode) {
        case ZYDIS_MACHINE_MODE_LEGACY_32:
            rd_kb_load(ctx, "arch/x86_32/callconv");
            break;

        case ZYDIS_MACHINE_MODE_LONG_64:
            rd_kb_load(ctx, "arch/x86_64/callconv");
            break;

        default: break;
    }
}

static const char* x86_get_mnemonic(const RDInstruction* instr,
                                    RDProcessor* p) {
    RD_UNUSED(p);
    return ZydisMnemonicGetString((ZydisMnemonic)instr->id);
}

static void x86_register_processor(RDProcessorPlugin* plugin,
                                   ZydisMachineMode mode, ZydisStackWidth width,
                                   const char* id, const char* name,
                                   int addrsize) {
    X86UserData* ud = rd_alloc(sizeof(*ud));
    ud->mode = mode;
    ud->width = width;

    plugin->level = RD_API_LEVEL;
    plugin->id = id;
    plugin->name = name;
    plugin->ptr_size = addrsize;
    plugin->userdata = ud;
    plugin->decode = x86_decode;
    plugin->encode = x86_encode;
    plugin->emulate = x86_emulate;
    plugin->lift = x86_lift;
    plugin->render_operand = x86_render_operand;
    plugin->create = x86_create;
    plugin->destroy = x86_destroy;
    plugin->setup = x86_setup;
    plugin->get_mnemonic = x86_get_mnemonic;
    plugin->query_reg = x86_query_reg;

    rd_register_processor(plugin);
}

static RDProcessorPlugin x86_16_real = {0};
static RDProcessorPlugin x86_16 = {0};
static RDProcessorPlugin x86_32 = {0};
static RDProcessorPlugin x86_64 = {0};

void rd_plugin_create(void) {
    x86_register_processor(&x86_16_real, ZYDIS_MACHINE_MODE_REAL_16,
                           ZYDIS_STACK_WIDTH_16, "x86_16_real", "X86_16 (Real)",
                           sizeof(u16));

    x86_register_processor(&x86_16, ZYDIS_MACHINE_MODE_LEGACY_16,
                           ZYDIS_STACK_WIDTH_16, "x86_16", "X86_16 (Protected)",
                           sizeof(u16));

    x86_register_processor(&x86_32, ZYDIS_MACHINE_MODE_LEGACY_32,
                           ZYDIS_STACK_WIDTH_32, "x86_32", "X86_32",
                           sizeof(u32));

    x86_register_processor(&x86_64, ZYDIS_MACHINE_MODE_LONG_64,
                           ZYDIS_STACK_WIDTH_64, "x86_64", "X86_64",
                           sizeof(u64));
}

void rd_plugin_destroy(void) {
    rd_free(x86_16_real.userdata);
    rd_free(x86_16.userdata);
    rd_free(x86_32.userdata);
    rd_free(x86_64.userdata);
}
