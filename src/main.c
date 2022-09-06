#include "common.h"
#include "emu6502.h"
#include "opcode.h"

#include <ncurses.h>

typedef struct MemWriter {
  u8 *mem;
  u16 pointer;
} MemWriter;

MemWriter memw_init(u8 *mem) {
  MemWriter memw = {mem, 0xFFFC};
  return memw;
}

void mem_write_byte(MemWriter *memw, u8 byte) {
  memw->mem[memw->pointer] = byte;
  memw->pointer++;
}

void mem_write_word(MemWriter *memw, u16 word) {
  memw->mem[memw->pointer] = (u8)(word >> 8);
  memw->pointer++;
  memw->mem[memw->pointer] = (u8)word;
  memw->pointer++;
}

i32 main(i32 argc, char *argv[]) {
  Emulator emu;
  emu_init(&emu);
  MemWriter memw = memw_init(emu.mem);

  mem_write_byte(&memw, OPCODE_JMP_ABS);
  mem_write_word(&memw, 0x0800);

  memw.pointer = 0x0800;
  mem_write_byte(&memw, OPCODE_LDY_IM);
  mem_write_byte(&memw, 0x00);
  mem_write_byte(&memw, OPCODE_LDA_INDY);
  mem_write_word(&memw, 0x1000);

  memw.pointer = 0x1000;
  mem_write_word(&memw, 0x2000);

  memw.pointer = 0x2000;
  mem_write_byte(&memw, -42);

  initscr();
  noecho();
  curs_set(0);
  while (emu.is_running) {
    clear();
    emu_tick(&emu, true);
    getch();
  }
  endwin();
  return 0;
}
