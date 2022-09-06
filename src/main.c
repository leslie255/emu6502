#include "common.h"
#include "emu6502.h"
#include "opcode.h"

#include <ncurses.h>
#include <unistd.h>

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
  mem_write_byte(&memw, OPCODE_LDY_IM); // 0800
  mem_write_byte(&memw, 0x00);          // 0801
  mem_write_byte(&memw, OPCODE_LDA_IM);
  mem_write_byte(&memw, 0xFF);
  mem_write_byte(&memw, OPCODE_LDA_IM);
  mem_write_byte(&memw, 0xF0);
  mem_write_byte(&memw, OPCODE_LDA_INDY);
  mem_write_word(&memw, 0x1000);
  mem_write_byte(&memw, OPCODE_LDA_IM);
  mem_write_byte(&memw, 0xEE);
  mem_write_byte(&memw, OPCODE_LDA_IM);
  mem_write_byte(&memw, 0xE0);

  memw.pointer = 0x1000;
  mem_write_word(&memw, 0x2000);

  memw.pointer = 0x2000;
  mem_write_byte(&memw, -42);

  initscr();
  curs_set(0);
  while (true) {
    clear();
    emu_tick(&emu, true);
    refresh();
	if (!emu.is_running) {
	  break;
	}
    printw("\nPress n to continue to tick 1 instruction\n");
    while (getch() != 'n')
      ;
  }
  endwin();
  return 0;
}
