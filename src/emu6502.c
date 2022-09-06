#include "emu6502.h"

#include <ncurses.h>

static char log_buffer[1024] = {0};

void mem_init(u8 *mem) { bzero(mem, MEM_SIZE); }

void cpu_reset(CPU *cpu) {
  cpu->pc = 0xFFFC;
  cpu->sp = 0x0100;
  cpu->flag_c = 0;
  cpu->flag_z = 0;
  cpu->flag_i = 0;
  cpu->flag_d = 0;
  cpu->flag_b = 0;
  cpu->flag_v = 0;
  cpu->flag_n = 0;
}

u8 cpu_stat_get_byte(CPU *cpu) {
  u8 stat = 0;
  stat |= cpu->flag_c << 6;
  stat |= cpu->flag_z << 5;
  stat |= cpu->flag_i << 4;
  stat |= cpu->flag_d << 3;
  stat |= cpu->flag_b << 2;
  stat |= cpu->flag_v << 1;
  stat |= cpu->flag_n;

  return stat;
}

void cpu_set_stat_from_byte(CPU *cpu, u8 stat) {
  cpu->flag_c = (stat & 0x01000000) >> 6;
  cpu->flag_z = (stat & 0x00100000) >> 5;
  cpu->flag_i = (stat & 0x00010000) >> 4;
  cpu->flag_d = (stat & 0x00001000) >> 3;
  cpu->flag_b = (stat & 0x00000100) >> 2;
  cpu->flag_v = (stat & 0x00000010) >> 1;
  cpu->flag_n = stat & 0x00000001;
}

char zero_or_one(u8 x) {
  if (x == 0) {
    return '0';
  } else {
    return '1';
  }
}

void cpu_debug_print(CPU *cpu) {
  printw("REG\tHEX\tDEC(u)\tDEC(i)\n"
         "PC:\t%04X\t%u\t%d\n"
         "SP:\t%04X\t%u\t%d\n"
         "A:\t%02X\t%u\t%d\n"
         "X:\t%02X\t%u\t%d\n"
         "Y:\t%02X\t%u\t%d\n"
         "C   Z   I   D   B   V   N \n"
         "%c   %c   %c   %c   %c   %c   %c\n",
         cpu->pc, cpu->pc, (i16)cpu->pc, cpu->sp, cpu->sp, (i16)cpu->sp, cpu->a,
         cpu->a, (i8)cpu->a, cpu->x, cpu->x, (i8)cpu->x, cpu->y, cpu->y,
         (i8)cpu->y, zero_or_one(cpu->flag_c), zero_or_one(cpu->flag_z),
         zero_or_one(cpu->flag_i), zero_or_one(cpu->flag_d),
         zero_or_one(cpu->flag_b), zero_or_one(cpu->flag_v),
         zero_or_one(cpu->flag_n));
}

void emu_init(Emulator *emu) {
  cpu_reset(&emu->cpu);
  mem_init(emu->mem);
  emu->cycles = 0;
  emu->is_running = true;
}

// fetch 1 byte from memory on position of PC
u8 emu_fetch_byte(Emulator *emu) {
  u8 data = emu->mem[emu->cpu.pc];
  emu->cpu.pc++;
  return data;
}

u16 swap_bytes(u16 data) {
  return ((data << 8) & 0xff00) | ((data >> 8) & 0x00ff);
}

// fetch 2 bytes from memory on position of PC
// swap bytes if host is big endian
u16 emu_fetch_word(Emulator *emu) {
  // 6502 uses little endian
  u16 data = emu->mem[emu->cpu.pc];
  emu->cpu.pc++;
  data |= emu->mem[emu->cpu.pc] << 8;
  emu->cpu.pc++;

  if (BIG_ENDIAN) {
    data = swap_bytes(data);
  }

  return data;
}

void emu_update_flags_a(Emulator *emu) {
  emu->cpu.flag_z = (emu->cpu.a == 0);
  emu->cpu.flag_n = ((emu->cpu.a & 0b10000000) > 0);
}

void emu_update_flags_x(Emulator *emu) {
  emu->cpu.flag_z = (emu->cpu.x == 0);
  emu->cpu.flag_n = ((emu->cpu.x & 0b10000000) > 0);
}

void emu_update_flags_y(Emulator *emu) {
  emu->cpu.flag_z = (emu->cpu.y == 0);
  emu->cpu.flag_n = ((emu->cpu.y & 0b10000000) > 0);
}

void emu_print_stack(Emulator *emu) {
  printw("\t_0 _1 _2 _3 _4 _5 _6 _7 _8 _9 _A _B _C _D _E _F\n");
  for (usize i = 0x0100; i <= 0x01FF; i += 16) {
    printw("%04X\t%02X %02X %02X %02X %02X %02X %02X %02X "
           "%02X %02X %02X %02X %02X %02X %02X %02X\n",
           (u16)i, emu->mem[i], emu->mem[i + 1], emu->mem[i + 2],
           emu->mem[i + 3], emu->mem[i + 4], emu->mem[i + 5], emu->mem[i + 6],
           emu->mem[i + 7], emu->mem[i + 8], emu->mem[i + 9], emu->mem[i + 10],
           emu->mem[i + 11], emu->mem[i + 12], emu->mem[i + 13],
           emu->mem[i + 14], emu->mem[i + 15]);
  }
}

u8 emu_read_mem_byte(Emulator *emu, u16 addr) { return emu->mem[addr]; }

u16 emu_read_mem_word(Emulator *emu, u16 addr) {
  // 6502 uses little endian
  u16 data = emu->mem[addr];
  data |= emu->mem[addr + 1] << 8;

  if (BIG_ENDIAN) {
    data = swap_bytes(data);
  }

  return data;
}

void emu_tick(Emulator *emu, bool debug_output) {

#define PRINT_STAT(...)                                                        \
  if (debug_output) {                                                          \
    printw(""__VA_ARGS__);                                                     \
    printw("Opcode:\t0x%02X\n"                                                 \
           "Addr:\t0x%04X\n"                                                   \
           "Cycles:\t%llu\n"                                                   \
           "CPU status:\n",                                                    \
           opcode, emu->cpu.pc - 1, emu->cycles);                              \
    cpu_debug_print(&emu->cpu);                                                \
    printw("Stack:\n");                                                        \
    emu_print_stack(emu);                                                      \
    printw("log: -----------\n%s\n----------\n", log_buffer);                  \
    bzero(log_buffer, sizeof(log_buffer));                                     \
  }

  u8 opcode = 0x00;
  opcode = emu_fetch_byte(emu);

  switch (opcode) {

    // BRK
  case OPCODE_BRK: {
    PRINT_STAT("Force Interrupt (BRK) hasn't been properly implemented yet\n");
    emu->is_running = false;
  } break;

    // INX
  case OPCODE_INX: {
    emu->cpu.x++;
    emu_update_flags_x(emu);
    emu->cycles += 2;
  } break;

    // INY
  case OPCODE_INY: {
    emu->cpu.y++;
    emu_update_flags_y(emu);
    emu->cycles += 2;
  } break;

    // JMP
  case OPCODE_JMP_ABS: {
    u16 addr = emu_fetch_word(emu);
    sprintf(log_buffer, "JMP_ABS: 0x%04x", addr);
    emu->cpu.pc = addr;
    emu->cycles += 3;
  } break;

  case OPCODE_JMP_IND: {
    u16 addr0 = emu_fetch_word(emu);
    u16 addr = emu_read_mem_word(emu, addr0);
    emu->cpu.pc = addr;
    emu->cycles += 5;
  } break;

    // NOP
  case OPCODE_NOP: {
  } break;

    // LDA
  case OPCODE_LDA_IM: {
    u8 data = emu_fetch_byte(emu);
    emu->cpu.a = data;
    emu_update_flags_a(emu);
    emu->cycles += 2;
  } break;

  case OPCODE_LDA_ZP: {
    u8 addr = emu_fetch_byte(emu);
    emu->cpu.a = emu_read_mem_byte(emu, addr);
    emu_update_flags_a(emu);
    emu->cycles += 3;
  } break;

  case OPCODE_LDA_ZPX: {
    u8 addr = emu_fetch_byte(emu);
    addr += emu->cpu.x;
    emu->cpu.a = emu_read_mem_byte(emu, addr);
    emu_update_flags_a(emu);
    emu->cycles += 4;
  } break;

  case OPCODE_LDA_ABS: {
    u16 addr = emu_fetch_word(emu);
    emu->cpu.a = emu_read_mem_byte(emu, addr);
    emu_update_flags_a(emu);
    emu->cycles += 4;
  } break;

  case OPCODE_LDA_ABSX: {
    u16 addr = emu_fetch_word(emu);
    u16 new_addr = addr + emu->cpu.x;
    if ((addr & 0xFF00) != (new_addr & 0xFF00)) {
      // page changes, add one cycle
      emu->cycles++;
    }
    emu->cpu.a = emu_read_mem_byte(emu, new_addr);
    emu_update_flags_a(emu);
    emu->cycles += 4;
  } break;

  case OPCODE_LDA_ABSY: {
    u16 addr = emu_fetch_word(emu);
    u16 new_addr = addr + emu->cpu.y;
    if ((addr & 0xFF00) != (new_addr & 0xFF00)) {
      // page changes, add one cycle
      emu->cycles++;
    }
    emu->cpu.a = emu_read_mem_byte(emu, new_addr);
    emu_update_flags_a(emu);
    emu->cycles += 4;
  } break;

  case OPCODE_LDA_INDX: {
    u16 addr0 = emu_fetch_byte(emu) + emu->cpu.x;
    u16 addr = emu_read_mem_word(emu, addr0);
    emu->cpu.a = emu_read_mem_byte(emu, addr);
    emu_update_flags_a(emu);
    emu->cycles += 6;
  } break;

  case OPCODE_LDA_INDY: {
    u16 addr0 = emu_fetch_word(emu);
    u16 addr1 = emu_read_mem_word(emu, addr0);
    u16 addr = addr1 + emu->cpu.y;
    if ((addr1 & 0xFF00) != (addr & 0xFF00)) {
      // if page changes, add one cycle
      emu->cycles++;
    }
    emu->cpu.a = emu_read_mem_byte(emu, addr);
    emu_update_flags_a(emu);
    emu->cycles += 5;
    sprintf(log_buffer, "LDA_INDY:\naddr0:\t%04X\naddr1:\t%04X\naddr:\t%04X",
            addr0, addr1, addr);
  } break;

    // LDX
  case OPCODE_LDX_IM: {
    u8 data = emu_fetch_byte(emu);
    emu->cpu.x = data;
    emu_update_flags_x(emu);
    emu->cycles += 2;
  } break;

  case OPCODE_LDX_ZP: {
    u8 addr = emu_fetch_byte(emu);
    emu->cpu.x = emu_read_mem_byte(emu, addr);
    emu_update_flags_x(emu);
    emu->cycles += 3;
  } break;

  case OPCODE_LDX_ZPY: {
    u8 addr = emu_fetch_byte(emu);
    addr += emu->cpu.y;
    emu->cpu.x = emu_read_mem_byte(emu, addr);
    emu_update_flags_x(emu);
    emu->cycles += 4;
  } break;

  case OPCODE_LDX_ABS: {
    u16 addr = emu_fetch_word(emu);
    emu->cpu.x = emu_read_mem_byte(emu, addr);
    emu_update_flags_x(emu);
    emu->cycles += 4;
  } break;

  case OPCODE_LDX_ABSY: {
    u16 addr = emu_fetch_word(emu);
    u16 new_addr = addr + emu->cpu.y;
    if ((addr & 0xFF00) != (new_addr & 0xFF00)) {
      // page changes, add one cycle
      emu->cycles++;
    }
    emu->cpu.x = emu_read_mem_byte(emu, new_addr);
    emu_update_flags_x(emu);
    emu->cycles += 4;
  } break;

    // LDY
  case OPCODE_LDY_IM: {
    u8 data = emu_fetch_byte(emu);
    emu->cpu.y = data;
    emu_update_flags_y(emu);
    emu->cycles += 2;
  } break;

  case OPCODE_LDY_ZP: {
    u8 addr = emu_fetch_byte(emu);
    emu->cpu.y = emu_read_mem_byte(emu, addr);
    emu_update_flags_y(emu);
    emu->cycles += 3;
  } break;

  case OPCODE_LDY_ZPY: {
    u8 addr = emu_fetch_byte(emu);
    addr += emu->cpu.y;
    emu->cpu.y = emu_read_mem_byte(emu, addr);
    emu_update_flags_y(emu);
    emu->cycles += 4;
  } break;

  case OPCODE_LDY_ABS: {
    u16 addr = emu_fetch_word(emu);
    emu->cpu.y = emu_read_mem_byte(emu, addr);
    emu_update_flags_y(emu);
    emu->cycles += 4;
  } break;

  case OPCODE_LDY_ABSY: {
    u16 addr = emu_fetch_word(emu);
    u16 new_addr = addr + emu->cpu.y;
    if ((addr & 0xFF00) != (new_addr & 0xFF00)) {
      // page changes, add one cycle
      emu->cycles++;
    }
    emu->cpu.y = emu_read_mem_byte(emu, new_addr);
    emu_update_flags_y(emu);
    emu->cycles += 4;
  } break;

    // PHA
  case OPCODE_PHA: {
    emu->cpu.sp++;
    emu->mem[emu->cpu.sp] = emu->cpu.a;
    emu->cycles += 3;
  } break;

    // PHP
  case OPCODE_PHP: {
    emu->cpu.sp++;
    emu->mem[emu->cpu.sp] = cpu_stat_get_byte(&emu->cpu);
    emu->cycles += 3;
  } break;

    // PLA
  case OPCODE_PLA: {
    emu->cpu.a = emu_read_mem_byte(emu, emu->cpu.sp);
    emu->cpu.sp--;
    emu->cycles += 4;
  } break;

    // PLP
  case OPCODE_PLP: {
    u8 stat = emu_read_mem_byte(emu, emu->cpu.sp);
    cpu_set_stat_from_byte(&emu->cpu, stat);
    emu->cpu.sp--;
    emu->cycles += 4;
  } break;

    // STA
  case OPCODE_STA_ZP: {
    u16 addr = emu_fetch_byte(emu);
    emu->mem[addr] = emu->cpu.a;
    emu->cycles += 3;
  } break;

  case OPCODE_STA_ZPX: {
    u16 addr = emu_fetch_byte(emu) + emu->cpu.x;
    emu->mem[addr] = emu->cpu.a;
    emu->cycles += 4;
  } break;

  case OPCODE_STA_ABS: {
    u16 addr = emu_fetch_word(emu);
    emu->mem[addr] = emu->cpu.a;
    emu->cycles += 4;
  } break;

  case OPCODE_STA_ABSX: {
    u16 addr = emu_fetch_word(emu) + emu->cpu.x;
    emu->mem[addr] = emu->cpu.a;
    emu->cycles += 5;
  } break;

  case OPCODE_STA_ABSY: {
    u16 addr = emu_fetch_word(emu) + emu->cpu.y;
    emu->mem[addr] = emu->cpu.a;
    emu->cycles += 5;
  } break;

  case OPCODE_STA_INDX: {
    u16 addr0 = emu_fetch_word(emu) + emu->cpu.x;
    u16 addr = emu_read_mem_word(emu, addr0);
    emu->mem[addr] = emu->cpu.a;
    emu->cycles += 6;
  } break;

  case OPCODE_STA_INDY: {
    u16 addr0 = emu_fetch_word(emu);
    u16 addr = emu_read_mem_word(emu, addr0) + emu->cpu.y;
    emu->mem[addr] = emu->cpu.a;
    emu->cycles += 6;
  } break;

    // STX
  case OPCODE_STX_ZP: {
    u16 addr = emu_fetch_byte(emu);
    emu->mem[addr] = emu->cpu.x;
    emu->cycles += 3;
  } break;

  case OPCODE_STX_ZPY: {
    u16 addr = emu_fetch_byte(emu) + emu->cpu.y;
    emu->mem[addr] = emu->cpu.x;
    emu->cycles += 4;
  } break;

  case OPCODE_STX_ABS: {
    u16 addr = emu_fetch_word(emu);
    emu->mem[addr] = emu->cpu.x;
    emu->cycles += 4;
  } break;

    // STY
  case OPCODE_STY_ZP: {
    u16 addr = emu_fetch_byte(emu);
    emu->mem[addr] = emu->cpu.y;
    emu->cycles += 3;
  } break;

  case OPCODE_STY_ZPX: {
    u16 addr = emu_fetch_byte(emu) + emu->cpu.x;
    emu->mem[addr] = emu->cpu.y;
    emu->cycles += 4;
  } break;

  case OPCODE_STY_ABS: {
    u16 addr = emu_fetch_word(emu);
    emu->mem[addr] = emu->cpu.y;
    emu->cycles += 4;
  } break;

    // TAX
  case OPCODE_TAX: {
    emu->cpu.x = emu->cpu.a;
    emu_update_flags_x(emu);
    emu->cycles += 2;
  } break;

    // TAY
  case OPCODE_TAY: {
    emu->cpu.y = emu->cpu.a;
    emu_update_flags_y(emu);
    emu->cycles += 2;
  } break;

    // TSX
  case OPCODE_TSX: {
    emu->cpu.x = emu->cpu.sp;
    emu_update_flags_x(emu);
    emu->cycles += 2;
  } break;

    // TXA
  case OPCODE_TXA: {
    emu->cpu.a = emu->cpu.x;
    emu_update_flags_a(emu);
    emu->cycles += 2;
  } break;

    // TXS
  case OPCODE_TXS: {
    emu->cpu.sp = emu->cpu.x;
    emu->cycles += 2;
  } break;

    // TYA
  case OPCODE_TYA: {
    emu->cpu.a = emu->cpu.y;
    emu_update_flags_y(emu);
    emu->cycles += 2;
  } break;

  default: {
    emu->is_running = false;
  } break;
  }
  PRINT_STAT("", opcode);
  return;
}
