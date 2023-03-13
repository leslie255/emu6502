#include "common.h"
#include "emu6502.h"
#include "opcode.h"

#include <ncurses.h>
#include <unistd.h>

typedef struct MemWriter {
  u8 *mem;
  usize head;
} MemWriter;

MemWriter memw_init(u8 *mem) {
  MemWriter memw = {mem, 0xFFFC};
  return memw;
}

void mem_write_byte(MemWriter *memw, u8 byte) {
  memw->mem[memw->head] = byte;
  memw->head++;
}

void mem_write_word(MemWriter *memw, u16 word) {
  memw->mem[memw->head] = (u8)(word >> 8);
  memw->head++;
  memw->mem[memw->head] = (u8)word;
  memw->head++;
}

i32 main(i32 argc, char *argv[]) {
  Emulator emu;
  emu_init(&emu);
  MemWriter writer = memw_init(emu.mem);

  mem_write_byte(&writer, OPCODE_JSR_ABS);
  mem_write_word(&writer, 0x0800);

  writer.head = 0x0800;
  mem_write_byte(&writer, OPCODE_JSR_ABS);
  mem_write_word(&writer, 0x1000);
  mem_write_byte(&writer, OPCODE_NOP);
  mem_write_byte(&writer, OPCODE_NOP);
  mem_write_byte(&writer, OPCODE_NOP);
  mem_write_byte(&writer, OPCODE_NOP);
  mem_write_byte(&writer, OPCODE_NOP);
  mem_write_byte(&writer, OPCODE_NOP);

  writer.head = 0x1000;
  mem_write_byte(&writer, OPCODE_RTS);

  bool less_io = false;

  for (i32 i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--less-io") == 0) {
      less_io = true;
    } else {
      printf("invalid argument: %s", argv[i]);
    }
  }

  printf("initialized\n");

  if (less_io) {
    while (true) {
      if (emu.is_running) {
        if (emu.cycles % 8000000 == 0) {
          printf("%llu cycles\n", emu.cycles);
        }
        emu_tick(&emu, false);
      }
    }
  } else {
    initscr();
    noecho();
    while (true) {
      clear();
      emu_tick(&emu, true);
      refresh();
      if (emu.is_running) {
        printw("\nPress n to tick forward 1 instruction\n");
        while (getch() != 'n')
          ;
      } else {
        printw("\nEmulation halted, press q to quit\n");
        while (getch() != 'q')
          ;
        break;
      }
    }
    endwin();
  }

  return 0;
}
