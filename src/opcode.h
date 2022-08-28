// Force Interrupt
#define OPCODE_BRK 0x00

// LDA
#define OPCODE_LDA_IM 0xA9   // Immediate
#define OPCODE_LDA_ZP 0xA5   // Zero Page
#define OPCODE_LDA_ZPX 0xB5  // Zero Page X
#define OPCODE_LDA_ABS 0xAD  // Absolute X
#define OPCODE_LDA_ABSX 0xBD // Absolute X
#define OPCODE_LDA_ABSY 0xB9 // Absolute Y
#define OPCODE_LDA_INDX 0xA1 // Indexed Indirect
#define OPCODE_LDA_INDY 0xB1 // Indirect Indexed

// LDX
#define OPCODE_LDX_IM 0xA2   // Immediate
#define OPCODE_LDX_ZP 0xA6   // Zero Page
#define OPCODE_LDX_ZPY 0xB6  // Zero Page Y
#define OPCODE_LDX_ABS 0xAE  // Absolute Y
#define OPCODE_LDX_ABSY 0xBE // Absolute Y

// LDY
#define OPCODE_LDY_IM 0xA0   // Immediate
#define OPCODE_LDY_ZP 0xA4   // Zero Page
#define OPCODE_LDY_ZPY 0xB4  // Zero Page Y
#define OPCODE_LDY_ABS 0xAC  // Absolute Y
#define OPCODE_LDY_ABSY 0xBC // Absolute Y

// JMP
#define OPCODE_JMP_ABS 0x4C // Absolute
#define OPCODE_JMP_IND 0x6C // Indirect
