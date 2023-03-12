// reference: https://www.masswerk.at/6502/6502_instruction_set.html

// IM:   Immediate
// ZP:   Zero Page
// ZPX:  Zero Page, X
// ZPY:  Zero Page, Y
// ABS:  Absolute
// ABSX: Absolute, X
// ABSY: Absolute, Y
// INDX: (Indirect, X)
// INDY: (Indirect), Y

#define OPCODE_ADC_IM   0x69
#define OPCODE_ADC_ZP   0x65
#define OPCODE_ADC_ZPX  0x75
#define OPCODE_ADC_ABS  0x6D
#define OPCODE_ADC_ABSX 0x7D
#define OPCODE_ADC_ABSY 0x79
#define OPCODE_ADC_INDX 0x61
#define OPCODE_ADC_INDY 0x71

#define OPCODE_BRK 0x00

#define OPCODE_CMP_IM 0xC9
#define OPCODE_CMP_ZP 0xC5
#define OPCODE_CMP_ZPX 0xD5
#define OPCODE_CMP_ABS 0xCD
#define OPCODE_CMP_ABSX 0xDD
#define OPCODE_CMP_ABSY 0xD9
#define OPCODE_CMP_INDX 0xC1
#define OPCODE_CMP_INDY 0xD1

#define OPCODE_CPX_IM 0xE0
#define OPCODE_CPX_ZP 0xE4
#define OPCODE_CPX_ABS 0xEC

#define OPCODE_CPY_IM 0xC0
#define OPCODE_CPY_ZP 0xC4
#define OPCODE_CPY_ABS 0xCC

#define OPCODE_DEC_ZP 0xC6
#define OPCODE_DEC_ZPX 0xD6
#define OPCODE_DEC_ABS 0xCE
#define OPCODE_DEC_ABSX 0xDE

#define OPCODE_DEX 0xCA

#define OPCODE_DEY 0x88

#define OPCODE_INC_ZP 0xE6
#define OPCODE_INC_ZPX 0xF6
#define OPCODE_INC_ABS 0xEE
#define OPCODE_INC_ABSX 0xFE

#define OPCODE_INX 0xE8

#define OPCODE_INY 0xC8

#define OPCODE_JMP_ABS 0x4C
#define OPCODE_JMP_IND 0x6C

#define OPCODE_JSR_ABS 0x20

#define OPCODE_NOP 0xEA

#define OPCODE_LDA_IM 0xA9
#define OPCODE_LDA_ZP 0xA5
#define OPCODE_LDA_ZPX 0xB5
#define OPCODE_LDA_ABS 0xAD
#define OPCODE_LDA_ABSX 0xBD
#define OPCODE_LDA_ABSY 0xB9
#define OPCODE_LDA_INDX 0xA1
#define OPCODE_LDA_INDY 0xB1

#define OPCODE_LDX_IM 0xA2
#define OPCODE_LDX_ZP 0xA6
#define OPCODE_LDX_ZPY 0xB6
#define OPCODE_LDX_ABS 0xAE
#define OPCODE_LDX_ABSY 0xBE

#define OPCODE_LDY_IM 0xA0
#define OPCODE_LDY_ZP 0xA4
#define OPCODE_LDY_ZPY 0xB4
#define OPCODE_LDY_ABS 0xAC
#define OPCODE_LDY_ABSY 0xBC

#define OPCODE_PHA 0x48

#define OPCODE_PHP 0x08

#define OPCODE_PLA 0x68

#define OPCODE_PLP 0x28

#define OPCODE_SEC 0x38

#define OPCODE_SED 0xF8

#define OPCODE_SEI 0x78

#define OPCODE_STA_ZP 0x85
#define OPCODE_STA_ZPX 0x95
#define OPCODE_STA_ABS 0x8D
#define OPCODE_STA_ABSX 0x9D
#define OPCODE_STA_ABSY 0x99
#define OPCODE_STA_INDX 0x81
#define OPCODE_STA_INDY 0x91

#define OPCODE_STX_ZP 0x86
#define OPCODE_STX_ZPY 0x96
#define OPCODE_STX_ABS 0x8E

#define OPCODE_STY_ZP 0x84
#define OPCODE_STY_ZPX 0x94
#define OPCODE_STY_ABS 0x8C

#define OPCODE_TAX 0xAA

#define OPCODE_TAY 0xA8

#define OPCODE_TSX 0xBA

#define OPCODE_TXA 0x8A

#define OPCODE_TXS 0x9A

#define OPCODE_TYA 0x98
