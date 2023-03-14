#include "calc.h"
#include "common.h"
#include "emu6502.h"
#include "opcode.h"

#include <ncurses.h>
#include <time.h>
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

  mem_write_byte(&writer, OPCODE_JMP_ABS);
  mem_write_word(&writer, 0x0800);

  writer.head = 0x0800;
  mem_write_byte(&writer, OPCODE_JSR_ABS);
  mem_write_word(&writer, 0x1020);
  mem_write_byte(&writer, OPCODE_JMP_ABS);
  mem_write_word(&writer, 0x0800);

  writer.head = 0x1020;
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
    clock_t prev_time = clock();
    u64 prev_cycles = 1;
    while (true) {
      if (!emu.is_running) {
        printf("Emulator halted at %llu cycles\n", emu.cycles);
        break;
      }
      if (emu.cycles % 128000000 == 0) {
        clock_t current_time = clock();
        f64 d = (f64)(current_time - prev_time) / (f64)CLOCKS_PER_SEC;
        f64 clock_speed = (f64)(emu.cycles - prev_cycles) * (1.0f / d) / 10000000.0f;
        printf("%.2lf\tMHz\n", clock_speed);
        prev_time = current_time;
        prev_cycles = emu.cycles;
      }
      emu_tick(&emu, false);
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
