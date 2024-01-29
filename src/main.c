#include "calc.h"
#include "common.h"
#include "emu6502.h"
#include "opcode.h"

#include <ncurses.h>
#include <stdio.h>
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

  bool dbg = false;

  for (i32 i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--dbg") == 0) {
      dbg = true;
    } else {
      printf("invalid argument: %s\n", argv[i]);
      return 1;
    }
  }

  Emulator emu;
  emu_init(&emu, dbg);
  MemWriter writer = memw_init(emu.mem);

  // starts on 0xFFFC by default
  mem_write_byte(&writer, OPCODE_JMP_ABS); // JMP 0x0800
  mem_write_word(&writer, 0x0800);

  writer.head = 0x0800;
  mem_write_byte(&writer, OPCODE_JSR_ABS); // JSR 0x1000
  mem_write_word(&writer, 0x1000);
  mem_write_byte(&writer, OPCODE_JMP_ABS); // JMP 0x0800
  mem_write_word(&writer, 0x0800);

  writer.head = 0x1000;
  mem_write_byte(&writer, OPCODE_LDA_IM); // LDA $0
  mem_write_byte(&writer, 0x00);
  mem_write_byte(&writer, OPCODE_SED);    // SED ; enable decimal mode
  mem_write_byte(&writer, OPCODE_ADC_IM); // ADC $1
  mem_write_byte(&writer, 0x01);
  mem_write_byte(&writer, OPCODE_BCS_REL); // BCS +4 ; branch if carry set
  mem_write_byte(&writer, 4);
  mem_write_byte(&writer, OPCODE_BCC_REL); // BCC +3 ; branch if carry clear
  mem_write_byte(&writer, 3);
  mem_write_byte(&writer, OPCODE_RTS);     // RTS
  mem_write_byte(&writer, OPCODE_JMP_ABS); // JMP 0x1000
  mem_write_word(&writer, 0x1000);

  printf("initialized\n");

  if (dbg) {
    initscr();
    noecho();
    while (true) {
      clear();
      emu_tick(&emu);
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
  } else {
    clock_t prev_time = clock();
    u64 prev_cycles = 1;
    while (true) {
      if (!emu.is_running) {
        printf("Emulator halted at %llu cycles\n", emu.cycles);
        break;
      }
      // sometimes in a repeating loop the number could not be reached because
      // some instructions have odd-numbered cycles, therefore requiring the
      // `... == 0 || ... == 1`
      if (emu.cycles % 370440000 == 0 || emu.cycles % 370440000 == 1) {
        clock_t current_time = clock();
        f64 d = (f64)(current_time - prev_time) / (f64)CLOCKS_PER_SEC;
        f64 clock_speed =
            (f64)(emu.cycles - prev_cycles) * (1.0f / d) / 10000000.0f;
        printf("%.2lf\tMHz\n", clock_speed);
        prev_time = current_time;
        prev_cycles = emu.cycles;
      }
      emu_tick(&emu);
    }
  }

  return 0;
}
