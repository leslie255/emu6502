#include "common.h"
#include "opcode.h"
#include "emu6502.h"

i32 main(i32 argc, char *argv[]) {
  Emulator emu;
  emu_init(&emu);

  emu.mem[0x0800] = OPCODE_LDA_IM;
  emu.mem[0x0801] = -42;

    // Starting point
  emu.mem[0xFFFC] = OPCODE_JMP_ABS;
  emu.mem[0xFFFD] = 0x00;
  emu.mem[0xFFFE] = 0x08;

  while (emu.is_running) {
    emu_tick(&emu, true);
  }

  return 0;
}
