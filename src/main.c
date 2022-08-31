#include "common.h"
#include "opcode.h"
#include "emu6502.h"

i32 main(i32 argc, char *argv[]) {
  Emulator emu;
  emu_init(&emu);

  emu.mem[0x0800] = OPCODE_LDA_IM;
  emu.mem[0x0801] = -42;
  emu.mem[0x0802] = OPCODE_PHP;
  emu.mem[0x0803] = OPCODE_LDA_IM;
  emu.mem[0x0804] = 0x00;
  emu.mem[0x0805] = OPCODE_JMP_ABS;
  emu.mem[0x0806] = 0x10;
  emu.mem[0x0807] = 0x08;
  emu.mem[0x0808] = OPCODE_PLP;
  emu.mem[0x0809] = OPCODE_LDA_IM;
  emu.mem[0x080A] = 0xAA;
  emu.mem[0x080B] = OPCODE_STA_ABS;
  emu.mem[0x080C] = 0x01;
  emu.mem[0x080D] = 0xFE;
  emu.mem[0x080E] = OPCODE_LDX_IM;
  emu.mem[0x080F] = 0x01;
  emu.mem[0x0810] = OPCODE_LDA_IM;
  emu.mem[0x0811] = 0xBB;
  emu.mem[0x0812] = OPCODE_STA_ABSX;
  emu.mem[0x0813] = 0x01;
  emu.mem[0x0814] = 0xFE;
  emu.mem[0x0815] = OPCODE_PHA;
  emu.mem[0x0816] = OPCODE_PHP;

    // Starting point
  emu.mem[0xFFFC] = OPCODE_JMP_ABS;
  emu.mem[0xFFFD] = 0x10;
  emu.mem[0xFFFE] = 0x00;

  while (emu.is_running) {
    emu_tick(&emu, true);
  }

  return 0;
}
