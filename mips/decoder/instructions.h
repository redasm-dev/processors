#pragma once

#include <redasm/redasm.h>

#define MIPS_OP_BITS 6

typedef enum {
    // R-Type
    MIPS_INSTR_ADD = 0,
    MIPS_INSTR_ADDU,
    MIPS_INSTR_AND,
    MIPS_INSTR_DIV,
    MIPS_INSTR_DIVU,
    MIPS_INSTR_MULT,
    MIPS_INSTR_MULTU,
    MIPS_INSTR_NOR,
    MIPS_INSTR_OR,
    MIPS_INSTR_SLL,
    MIPS_INSTR_SRA,
    MIPS_INSTR_SRL,
    MIPS_INSTR_SUB,
    MIPS_INSTR_SUBU,
    MIPS_INSTR_XOR,
    MIPS_INSTR_SLT,
    MIPS_INSTR_SLTU,
    MIPS_INSTR_JR,
    MIPS_INSTR_MFHI,
    MIPS_INSTR_MFLO,
    MIPS_INSTR_MTHI,
    MIPS_INSTR_MTLO,
    MIPS_INSTR_SLLV,
    MIPS_INSTR_SRAV,
    MIPS_INSTR_SRLV,
    MIPS_INSTR_XORI,
    MIPS_INSTR_JALR,

    // C-Type
    MIPS_INSTR_TEQ,
    MIPS_INSTR_TGE,

    // I-Type
    MIPS_INSTR_ADDI,
    MIPS_INSTR_ADDIU,
    MIPS_INSTR_ANDI,
    MIPS_INSTR_ORI,
    MIPS_INSTR_LUI,
    MIPS_INSTR_BEQ,
    MIPS_INSTR_BGEZ,
    MIPS_INSTR_BGTZ,
    MIPS_INSTR_BLEZ,
    MIPS_INSTR_BNE,
    MIPS_INSTR_LB,
    MIPS_INSTR_LBU,
    MIPS_INSTR_LH,
    MIPS_INSTR_LHU,
    MIPS_INSTR_LW,
    MIPS_INSTR_LWL,
    MIPS_INSTR_LWR,
    MIPS_INSTR_SB,
    MIPS_INSTR_SH,
    MIPS_INSTR_SW,
    MIPS_INSTR_SWL,
    MIPS_INSTR_SWR,
    MIPS_INSTR_LHI,
    MIPS_INSTR_LLO,
    MIPS_INSTR_SLTI,
    MIPS_INSTR_SLTIU,

    // J-Type
    MIPS_INSTR_J,
    MIPS_INSTR_JAL,

    // B-Type
    MIPS_INSTR_BREAK,
    MIPS_INSTR_SYSCALL,

    // C0-Type
    MIPS_INSTR_MFC0,
    MIPS_INSTR_MTC0,

    // C2-Type
    MIPS_INSTR_MFC2,
    MIPS_INSTR_MTC2,
    MIPS_INSTR_CFC2,
    MIPS_INSTR_CTC2,

    // CLS-Type
    MIPS_INSTR_LWC1,
    MIPS_INSTR_SWC1,
    MIPS_INSTR_LWC2,
    MIPS_INSTR_SWC2,

    // Macro Instructions
    MIPS_MACRO_LA,
    MIPS_MACRO_LI,
    MIPS_MACRO_MOVE,
    MIPS_MACRO_LHU,
    MIPS_MACRO_LW,
    MIPS_MACRO_SW,
    MIPS_MACRO_SH,
    MIPS_MACRO_B,
    MIPS_MACRO_BEQZ,
    MIPS_MACRO_BNEZ,
    MIPS_MACRO_NOP,

    MIPS_INSTRUCTIONS_COUNT
} MIPSInstructionId;

typedef struct UNKFormat {
    unsigned : 21;
    unsigned code : 5;
    unsigned op : 6;
} UNKFormat;

typedef struct RFormat {
    unsigned funct : 6;
    unsigned shamt : 5;
    unsigned rd : 5;
    unsigned rt : 5;
    unsigned rs : 5;
    unsigned op : 6;
} RFormat;

typedef struct CFormat {
    unsigned funct : 6;
    unsigned code : 10;
    unsigned rt : 5;
    unsigned rs : 5;
    unsigned op : 6;
} CFormat;

typedef struct IFormatUnsigned {
    unsigned imm : 16;
    unsigned rt : 5;
    unsigned rs : 5;
    unsigned op : 6;
} IFormatUnsigned;

typedef struct IFormatSigned {
    signed imm : 16;
    unsigned rt : 5;
    unsigned rs : 5;
    unsigned op : 6;
} IFormatSigned;

typedef struct JFormat {
    unsigned target : 26;
    unsigned op : 6;
} JFormat;

typedef struct BFormat {
    unsigned funct : 6;
    unsigned code : 20;
    unsigned op : 6;
} BFormat;

typedef struct C1IMMFormat {
    unsigned imm : 11;
    unsigned fs : 5;
    unsigned rt : 5;
    unsigned code : 5;
    unsigned op : 6;
} C1IMMFormat;

typedef struct C2ImplFormat {
    unsigned impl : 11;
    unsigned rd : 5;
    unsigned rt : 5;
    unsigned code : 5;
    unsigned op : 6;
} C2ImplFormat;

typedef struct C1SELFormat {
    unsigned sel : 3;
    unsigned : 8;
    unsigned rd : 5;
    unsigned rt : 5;
    unsigned code : 5;
    unsigned op : 6;
} C1SELFormat;

typedef C1SELFormat C0SELFormat;
typedef IFormatUnsigned CLSFormat;

typedef union MIPSInstruction {
    u32 word;
    u16 hword[2];
    u8 bytes[4];

    UNKFormat unk;

    RFormat r;
    CFormat c;
    IFormatUnsigned i_u;
    IFormatSigned i_s;
    JFormat j;
    BFormat b;

    // Coprocessors
    CLSFormat cls;

    C0SELFormat c0sel;

    C1IMMFormat c1imm;
    C1SELFormat c1sel;

    C2ImplFormat c2impl;
} MIPSInstruction;

static_assert(sizeof(u32) == sizeof(MIPSInstruction),
              "invalid MIPS instruction size");

const char* mips_get_mnemonic(usize id);
