// BRK
#define OPCODE_BRK 0x00
// INX
#define OPCODE_INX 0xE8
// INY
#define OPCODE_INY 0xC8
// JMP
#define OPCODE_JMP_ABS 0x4C // Absolute
#define OPCODE_JMP_IND 0x6C // Indirect
// NOP
#define OPCODE_NOP 0xEA
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
// PHA
#define OPCODE_PHA 0x48
// PHP
#define OPCODE_PHP 0x08
// PLA
#define OPCODE_PLA 0x68
// PLP
#define OPCODE_PLP 0x28
// STA
#define OPCODE_STA_ZP 0x85   // Zero Page
#define OPCODE_STA_ZPX 0x95  // Zero Page X
#define OPCODE_STA_ABS 0x8D  // Absolute
#define OPCODE_STA_ABSX 0x9D // Absolute X
#define OPCODE_STA_ABSY 0x99 // Absolute Y
#define OPCODE_STA_INDX 0x81 // Indirect X
#define OPCODE_STA_INDY 0x91 // Indirect Y
// STX
#define OPCODE_STX_ZP 0x86  // Zero Page
#define OPCODE_STX_ZPY 0x96 // Zero Page Y
#define OPCODE_STX_ABS 0x8E // Absolute
// STY
#define OPCODE_STY_ZP 0x84  // Zero Page
#define OPCODE_STY_ZPX 0x94 // Zero Page Y
#define OPCODE_STY_ABS 0x8C // Absolute
// TAX
#define OPCODE_TAX 0xAA
// TAY
#define OPCODE_TAY 0xA8
// TSX
#define OPCODE_TSX 0xBA
// TXA
#define OPCODE_TXA 0x8A
// TXS
#define OPCODE_TXS 0x9A
// TYA
#define OPCODE_TYA 0x98

