#include "xtensa.h"
#include "capstone.h"

#define XTENSA_PLUGIN(_id, _name, _init)                                       \
    {                                                                          \
        .level = RD_API_LEVEL,                                                 \
        .id = (_id),                                                           \
        .name = (_name),                                                       \
        .flags = RD_PF_LE,                                                     \
        .ptr_size = sizeof(u32),                                               \
        .userdata = (void*)&(_init),                                           \
        .create = capstone_plugin_create,                                      \
        .destroy = capstone_plugin_destroy,                                    \
        .decode = xtensa_decode,                                               \
        .emulate = xtensa_emulate,                                             \
        .render_operand = xtensa_render_operand,                               \
        .get_mnemonic = capstone_plugin_get_mnemonic,                          \
        .query_reg = capstone_plugin_query_reg,                                \
    }

static const CapstoneInitData XTENSA_ESP8266_INIT = {
    .arch = CS_ARCH_XTENSA,
    .mode = CS_MODE_LITTLE_ENDIAN | CS_MODE_XTENSA_ESP8266,
};

static const CapstoneInitData XTENSA_ESP32_INIT = {
    .arch = CS_ARCH_XTENSA,
    .mode = CS_MODE_LITTLE_ENDIAN | CS_MODE_XTENSA_ESP32,
};

static const CapstoneInitData XTENSA_ESP32S2_INIT = {
    .arch = CS_ARCH_XTENSA,
    .mode = CS_MODE_LITTLE_ENDIAN | CS_MODE_XTENSA_ESP32S2,
};

static void _xtensa_classify_flow(RDInstruction* instr, xtensa_insn id) {
    switch(id) {
        case XTENSA_INS_J:
        case XTENSA_INS_JX: instr->flow = RD_IF_JUMP; break;

        case XTENSA_INS_BEQ:
        case XTENSA_INS_BEQI:
        case XTENSA_INS_BEQZ:
        case XTENSA_INS_BF:
        case XTENSA_INS_BT:
        case XTENSA_INS_BGE:
        case XTENSA_INS_BGEI:
        case XTENSA_INS_BGEU:
        case XTENSA_INS_BGEUI:
        case XTENSA_INS_BGEZ:
        case XTENSA_INS_BLT:
        case XTENSA_INS_BLTI:
        case XTENSA_INS_BLTU:
        case XTENSA_INS_BLTUI:
        case XTENSA_INS_BLTZ:
        case XTENSA_INS_BNE:
        case XTENSA_INS_BNEI:
        case XTENSA_INS_BNEZ:
        case XTENSA_INS_BALL:
        case XTENSA_INS_BANY:
        case XTENSA_INS_BNALL:
        case XTENSA_INS_BNONE:
        case XTENSA_INS_BBC:
        case XTENSA_INS_BBCI:
        case XTENSA_INS_BBS:
        case XTENSA_INS_BBSI: instr->flow = RD_IF_JUMP_COND; break;

        case XTENSA_INS_LOOP:
        case XTENSA_INS_LOOPGTZ:
        case XTENSA_INS_LOOPNEZ: break;

        case XTENSA_INS_CALL0:
        case XTENSA_INS_CALL4:
        case XTENSA_INS_CALL8:
        case XTENSA_INS_CALL12:
        case XTENSA_INS_CALLX0:
        case XTENSA_INS_CALLX4:
        case XTENSA_INS_CALLX8:
        case XTENSA_INS_CALLX12: instr->flow = RD_IF_CALL; break;

        case XTENSA_INS_RET:
        case XTENSA_INS_RET_N:
        case XTENSA_INS_RETW:
        case XTENSA_INS_RETW_N:
        case XTENSA_INS_ILL:
        case XTENSA_INS_ILL_N: instr->flow = RD_IF_STOP; break;

        default: break;
    }
}

static void xtensa_decode(RDContext* ctx, RDInstruction* instr,
                          RDProcessor* p) {
    char data[sizeof(u32)];
    if(!rd_read(ctx, instr->address, data, rd_count_of(data))) return;

    const cs_insn* cs_insn =
        capstone_plugin_decode(instr, data, rd_count_of(data), p);
    if(!cs_insn) return;

    const cs_xtensa* d = &cs_insn->detail->xtensa;
    _xtensa_classify_flow(instr, (xtensa_insn)cs_insn->id);

    for(uint8_t i = 0; i < d->op_count && i < RD_MAX_OPERANDS; i++) {
        const cs_xtensa_op* cop = &d->operands[i];
        RDOperand* op = &instr->operands[i];

        switch(cop->type) {
            case XTENSA_OP_REG:
                op->kind = RD_OP_REG;
                op->reg = cop->reg;
                break;

            case XTENSA_OP_IMM: {
                if(rd_instr_is_branch(instr)) {
                    op->kind = RD_OP_ADDR;
                    op->addr = (RDAddress)cop->imm;
                }
                else {
                    op->kind = RD_OP_IMM;
                    op->s_imm = cop->imm;
                }
                break;
            }

            case XTENSA_OP_MEM:
                op->kind = RD_OP_DISPL;
                op->displ.base = cop->mem.base;
                op->displ.index = RD_REGID_INVALID;
                op->displ.offset = cop->mem.disp;
                break;

            case XTENSA_OP_L32R:
                op->kind = RD_OP_MEM;
                op->mem = (RDAddress)cop->imm;
                break;

            default: break;
        }
    }
}

static void xtensa_emulate(RDContext* ctx, const RDInstruction* instr,
                           RDProcessor* p) {
    RD_UNUSED(p);

    rd_foreach_operand(i, op, instr) {
        if(op->kind == RD_OP_ADDR) {
            if(rd_instr_is_call(instr))
                rd_add_xref(ctx, instr->address, op->addr, RD_CR_CALL);
            else if(rd_instr_is_jump(instr))
                rd_add_xref(ctx, instr->address, op->addr, RD_CR_JUMP);
            else
                rd_add_xref(ctx, instr->address, op->addr, RD_DR_ADDRESS);
        }
        else if(op->kind == RD_OP_MEM && rd_is_address(ctx, op->mem)) {
            rd_add_xref(ctx, instr->address, op->mem, RD_DR_READ);

            if(instr->id == XTENSA_INS_L32R)
                rd_auto_type(ctx, op->mem, "u32", 0, RD_TYPE_PTR);
        }
    }

    if(rd_instr_can_flow(instr)) rd_flow(ctx, instr->address + instr->length);
}

static bool xtensa_render_operand(RDRenderer* r, const RDInstruction* instr,
                                  int idx, RDProcessor* p) {
    RD_UNUSED(p);
    const RDOperand* op = &instr->operands[idx];

    if(op->kind == RD_OP_DISPL) {
        rd_renderer_norm(r, "[");
        rd_renderer_reg(r, op->displ.base);

        if(op->displ.offset != 0) {
            rd_renderer_norm(r, ", #");
            rd_renderer_num(r, op->displ.offset, 10, 0, RD_NUM_SIGNED);
        }

        rd_renderer_norm(r, "]");
        return true;
    }

    return false;
}

const RDProcessorPlugin XTENSA_ESP8266 =
    XTENSA_PLUGIN("xtensa_esp8266", "Xtensa (ESP8266)", XTENSA_ESP8266_INIT);

const RDProcessorPlugin XTENSA_ESP32 =
    XTENSA_PLUGIN("xtensa_esp32", "Xtensa (ESP32)", XTENSA_ESP32_INIT);

const RDProcessorPlugin XTENSA_ESP32S2 =
    XTENSA_PLUGIN("xtensa_esp32s2", "Xtensa (ESP32-S2)", XTENSA_ESP32S2_INIT);
