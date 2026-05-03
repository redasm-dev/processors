#pragma once

#include "decoder/instructions.h"
#include <redasm/redasm.h>

typedef enum { MIPS_VERSION_NONE, MIPS_V1 } MIPSVersion;

typedef enum {
    MIPS_FORMAT_NONE = 0,
    MIPS_FORMAT_R,
    MIPS_FORMAT_C,
    MIPS_FORMAT_I,
    MIPS_FORMAT_J,
    MIPS_FORMAT_B,
    MIPS_FORMAT_C0,
    MIPS_FORMAT_C1,
    MIPS_FORMAT_C2,
    MIPS_FORMAT_CLS,

    MIPS_FORMAT_MACRO,
    MIPS_FORMAT_COUNT
} MIPSFormat;

typedef enum {
    MIPS_CATEGORY_NONE = 0,

    MIPS_CATEGORY_LOAD,
    MIPS_CATEGORY_STORE,
    MIPS_CATEGORY_JUMP,
    MIPS_CATEGORY_JUMP_COND,
    MIPS_CATEGORY_CALL,
    MIPS_CATEGORY_RET,
} MIPSCategory;

typedef struct MIPSOpcode {
    MIPSInstructionId id;
    MIPSCategory category;
    MIPSFormat format;
    MIPSVersion version;
} MIPSOpcode;

typedef union MIPSMacroInstruction {
    struct {
        u8 dst;
        u8 src;
    } regs; // opcode reg, reg

    struct {
        unsigned reg : 5;
        union {
            RDAddress address;
            u64 u_imm64;
            i64 s_imm64;
            u32 u_imm32;
            i32 s_imm32;
            u16 u_imm16;
            i16 s_imm16;
        };
    } regimm; // opcode reg, imm

    struct {
        union {
            RDAddress address;
            u64 u_imm64;
            i64 s_imm64;
            u32 u_imm32;
            i32 s_imm32;
            u16 u_imm16;
            i16 s_imm16;
        };
        unsigned base : 5;
        unsigned rt : 5;
    } loadstore; // opcode reg, offset(base)
} MIPSMacroInstruction;

typedef struct MIPSDecodedInstruction {
    const MIPSOpcode* opcode;
    int length;

    union {
        MIPSInstruction instr;
        MIPSMacroInstruction macro;
    };
} MIPSDecodedInstruction;

#define MIPS_N_OPCODES (1 << MIPS_OP_BITS)

extern MIPSOpcode mips_opcodes_r[MIPS_N_OPCODES];
extern MIPSOpcode mips_opcodes_c[MIPS_N_OPCODES];
extern MIPSOpcode mips_opcodes_i[MIPS_N_OPCODES];
extern MIPSOpcode mips_opcodes_j[MIPS_N_OPCODES];
extern MIPSOpcode mips_opcodes_b[MIPS_N_OPCODES];
extern MIPSOpcode mips_opcodes_c0[MIPS_N_OPCODES];
extern MIPSOpcode mips_opcodes_c1[MIPS_N_OPCODES];
extern MIPSOpcode mips_opcodes_c2[MIPS_N_OPCODES];
extern MIPSOpcode mips_opcodes_cls[MIPS_N_OPCODES];

void mips_initialize_formats(void);
void mips_decode_r(const MIPSDecodedInstruction* dec, RDInstruction* instr);
void mips_decode_i(const MIPSDecodedInstruction* dec, RDInstruction* instr);
void mips_decode_j(const MIPSDecodedInstruction* dec, RDInstruction* instr);
void mips_decode_b(const MIPSDecodedInstruction* dec, RDInstruction* instr);

bool mips_has_delay_slot(usize id);
bool mips_decode_one_be(const RDContext* ctx, RDAddress address,
                        MIPSDecodedInstruction* dec);
bool mips_decode_one_le(const RDContext* ctx, RDAddress address,
                        MIPSDecodedInstruction* dec);

RDAddress mips_calc_addr16(RDAddress pc, u16 imm);
RDAddress mips_calc_addr26(RDAddress pc, u32 imm);
