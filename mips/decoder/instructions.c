#include "instructions.h"

// clang-format off
static const char* const MIPS_MNEMONICS[] = {
    // R-Type
    "add", "addu", "and", "div", "divu", "mult", "multu", "nor", "or",
    "sll", "sra", "srl", "sub", "subu", "xor", "slt", "sltu", "jr", "mfhi",
    "mflo", "mthi", "mtlo", "sllv", "srav", "srlv", "xori", "jalr",

    // C-Type
    "teq", "tge",

    // I-Type
    "addi", "addiu", "andi", "ori", "lui", "beq", "bgez", "bgtz", "blez",
    "bne", "lb", "lbu", "lh", "lhu", "lw", "lwl", "lwr", "sb", "sh", "sw",
    "swl", "swr", "lhi", "llo", "slti", "sltiu",

    // J-Type
    "j", "jal",

    // B-Type
    "break", "syscall",

    // C0-Type
    "mfc0", "mtc0",

    // C2-Type
    "mfc2", "mtc2", "cfc2", "ctc2",

    // CLS-Type
    "lwc1", "swc1", "lwc2", "swc2",

    // Macro Instructions
    "la", "li", "move", "lhu", "lw", "sw", "sh", "b", "beqz", "bnez",
    "nop",
};
// clang-format on

#define MIPS_MNEMONICS_COUNT (sizeof(MIPS_MNEMONICS) / sizeof(*MIPS_MNEMONICS))

static_assert(MIPS_MNEMONICS_COUNT == MIPS_INSTRUCTIONS_COUNT,
              "mnemonics out of sync");

const char* mips_get_mnemonic(usize id) {
    return id < MIPS_INSTRUCTIONS_COUNT ? MIPS_MNEMONICS[id] : "???";
}
