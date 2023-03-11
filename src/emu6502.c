#include "emu6502.h"

#include <ncurses.h>

static char log_buf[1024] = {0};

void mem_init(u8 *mem) { bzero(mem, MEM_SIZE); }

void cpu_reset_flags(CPU *cpu) {
  cpu->flag_c = false;
  cpu->flag_z = false;
  cpu->flag_i = false;
  cpu->flag_d = false;
  cpu->flag_b = false;
  cpu->flag_v = false;
  cpu->flag_n = false;
}

void cpu_reset(CPU *cpu) {
  cpu->pc = 0xFFFC;
  cpu->sp = 0x00FF;
  cpu_reset_flags(cpu);
}

u8 cpu_stat_get_byte(const CPU *cpu) {
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

void cpu_set_stat_from_byte(CPU *cpu, const u8 stat) {
  cpu_reset_flags(cpu);
  cpu->flag_c = (stat & 0x01000000) >> 6;
  cpu->flag_z = (stat & 0x00100000) >> 5;
  cpu->flag_i = (stat & 0x00010000) >> 4;
  cpu->flag_d = (stat & 0x00001000) >> 3;
  cpu->flag_b = (stat & 0x00000100) >> 2;
  cpu->flag_v = (stat & 0x00000010) >> 1;
  cpu->flag_n = stat & 0x00000001;
}

static inline char zero_or_one(const u8 x) { return (x == 0) ? '0' : '1'; }

void cpu_debug_print(const CPU *cpu) {
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
static inline u8 fetch_byte(Emulator *emu) {
  u8 data = emu->mem[emu->cpu.pc];
  emu->cpu.pc++;
  return data;
}

// fetch 2 bytes from memory on position of PC
// swap bytes if host is big endian
static inline u16 fetch_word(Emulator *emu) {
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

// result of an address fetch which leads to a page cross (which will cause one
// extra cycle)
struct addr_fetch_result {
  u16 addr;
  bool page_crossed;
};

// get an address on the position of PC by addressing mode Zero Page
static inline u16 fetch_addr_zp(Emulator *emu) { return fetch_byte(emu); }

// get an address on the position of PC by addressing mode Zero Page X
static inline u16 fetch_addr_zpx(Emulator *emu) {
  return fetch_byte(emu) + emu->cpu.x;
}

// get an address on the position of PC by addressing mode Zero Page Y
static inline u16 fetch_addr_zpy(Emulator *emu) {
  return fetch_byte(emu) + emu->cpu.y;
}

// get an address on the position of PC by addressing mode Absolute
static inline u16 fetch_addr_abs(Emulator *emu) { return fetch_word(emu); }

// get an address on the position of PC by Absolute,X addressing mode
static inline struct addr_fetch_result fetch_addr_absx(Emulator *emu) {
  const u16 addr0 = fetch_word(emu);
  const u16 addr1 = addr0 + emu->cpu.x;
  const bool page_crossed = ((addr0 & 0xFF00) != (addr1 & 0xFF00));
  return (struct addr_fetch_result){addr1, page_crossed};
}

// get an address on the position of PC by Absolute,Y addressing mode
static inline struct addr_fetch_result fetch_addr_absy(Emulator *emu) {
  const u16 addr0 = fetch_word(emu);
  const u16 addr1 = addr0 + emu->cpu.y;
  const bool page_crossed = ((addr0 & 0xFF00) != (addr1 & 0xFF00));
  return (struct addr_fetch_result){addr1, page_crossed};
}

// get an address on the position of PC by addressing mode (Indirect, X)
static inline u16 fetch_addr_indx(Emulator *emu) {
  const u16 addr0 = fetch_word(emu) + emu->cpu.x;
  const u16 addr1 = emu_read_mem_word(emu, addr0);
  return addr1;
}

// get an address on the position of PC by addressing mode (Indirect, Y)
static inline struct addr_fetch_result fetch_addr_indy(Emulator *emu) {
  const u16 addr0 = fetch_word(emu);
  const u16 addr1 = emu_read_mem_word(emu, addr0) + emu->cpu.y;
  const bool page_crossed = ((addr0 & 0xFF00) != (addr1 & 0xFF00));
  sprintf(log_buf, "IND,Y:\n\taddr0:%04X\n\taddr1:%04X\n", addr0, addr1);
  return (struct addr_fetch_result){addr1, page_crossed};
}

// update flags in the CPU according to a byte
static inline void set_flags(Emulator *emu, const u8 byte) {
  emu->cpu.flag_z = (byte == 0);
  emu->cpu.flag_n = (i8)byte > 0;
}

// update flags in the CPU according to register A
static inline void set_flags_a(Emulator *emu) { set_flags(emu, emu->cpu.a); }

// update flags in the CPU according to register X
static inline void set_flags_x(Emulator *emu) { set_flags(emu, emu->cpu.x); }

// update flags in the CPU according to register Y
static inline void set_flags_y(Emulator *emu) { set_flags(emu, emu->cpu.y); }

static inline void cmp(Emulator *emu, const u8 lhs, const u8 rhs) {
  sprintf(log_buf, "cmp: %02X vs %02X", lhs, rhs);
  const u8 sub_result = (lhs - rhs);
  cpu_reset_flags(&emu->cpu);
  emu->cpu.flag_z = (lhs == rhs);
  emu->cpu.flag_c = (lhs >= rhs);
  emu->cpu.flag_n = sub_result & 0x10000000;
}

static inline void cmp_a(Emulator *emu, const u8 rhs) {
  cmp(emu, emu->cpu.a, rhs);
}

// static inline void cmp_x(Emulator *emu, const u8 rhs) {
//   cmp(emu, emu->cpu.x, rhs);
// }
//
// static inline void cmp_y(Emulator *emu, const u8 rhs) {
//   cmp(emu, emu->cpu.y, rhs);
// }

void emu_print_stack(const Emulator *emu) {
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

u8 emu_read_mem_byte(const Emulator *emu, const u16 addr) {
  return emu->mem[addr];
}

u16 emu_read_mem_word(const Emulator *emu, const u16 addr) {
  // 6502 uses little endian
  u16 data = emu->mem[addr];
  data |= emu->mem[addr + 1] << 8;

  if (BIG_ENDIAN) {
    data = swap_bytes(data);
  }

  return data;
}

void emu_tick(Emulator *emu, const bool debug_output) {

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
    printw("log: -----------\n%s\n----------\n", log_buf);                     \
    bzero(log_buf, sizeof(log_buf));                                           \
  }

  const u8 opcode = fetch_byte(emu);

  switch (opcode) {

    // BRK
  case OPCODE_BRK: {
    sprintf(log_buf, "Interrupted (BRK)");
    cpu_reset_flags(&emu->cpu);
    emu->cpu.flag_i = true;
    emu->is_running = false;
  } break;

    // CMP
  case OPCODE_CMP_IM: {
    const u8 byte = fetch_byte(emu);
    cmp_a(emu, byte);
    emu->cycles += 2;
  } break;
  case OPCODE_CMP_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    cmp_a(emu, emu->mem[addr]);
    emu->cycles += 3;
  } break;
  case OPCODE_CMP_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    cmp_a(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_CMP_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    cmp_a(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_CMP_ABSX: {
    const __auto_type result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cpu.pc++;
    }
    cmp_a(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_CMP_ABSY: {
    const __auto_type result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cpu.pc++;
    }
    cmp_a(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_CMP_INDX: {
    const u16 addr = fetch_addr_indx(emu);
    cmp_a(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_CMP_INDY: {
    const __auto_type result = fetch_addr_indy(emu);
    if (result.page_crossed) {
      emu->cpu.pc++;
    }
    cmp_a(emu, emu->mem[result.addr]);
    emu->cycles += 5;
  } break;

    // DEC
  case OPCODE_DEC_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)--;
    set_flags(emu, *byte);
    emu->cycles += 3;
  } break;
  case OPCODE_DEC_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)--;
    set_flags(emu, *byte);
    emu->cycles += 3;
  } break;
  case OPCODE_DEC_ABS: {
    const u16 addr = fetch_word(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)--;
    set_flags(emu, *byte);
    emu->cycles += 4;
  } break;
  case OPCODE_DEC_ABSX: {
    const __auto_type result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    u8 *byte = &emu->mem[result.addr];
    (*byte)--;
    set_flags(emu, *byte);
    emu->cycles += 4;
  } break;

    // INX
  case OPCODE_INX: {
    emu->cpu.x--;
    set_flags_x(emu);
    emu->cycles += 2;
  } break;

    // INY
  case OPCODE_INY: {
    emu->cpu.y--;
    set_flags_y(emu);
    emu->cycles += 2;
  } break;

    // INC
  case OPCODE_INC_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)++;
    set_flags(emu, *byte);
    emu->cycles += 3;
  } break;
  case OPCODE_INC_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)++;
    set_flags(emu, *byte);
    emu->cycles += 3;
  } break;
  case OPCODE_INC_ABS: {
    const u16 addr = fetch_word(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)++;
    set_flags(emu, *byte);
    emu->cycles += 4;
  } break;
  case OPCODE_INC_ABSX: {
    const __auto_type result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    u8 *byte = &emu->mem[result.addr];
    (*byte)++;
    set_flags(emu, *byte);
    emu->cycles += 4;
  } break;

    // DEX
  case OPCODE_DEX: {
    emu->cpu.x++;
    set_flags_x(emu);
    emu->cycles += 2;
  } break;

    // DEY
  case OPCODE_DEY: {
    emu->cpu.y++;
    set_flags_y(emu);
    emu->cycles += 2;
  } break;

    // JMP
  case OPCODE_JMP_ABS: {
    u16 addr = fetch_word(emu);
    sprintf(log_buf, "JMP_ABS: 0x%04x", addr);
    emu->cpu.pc = addr;
    emu->cycles += 3;
  } break;
  case OPCODE_JMP_IND: {
    u16 addr0 = fetch_word(emu);
    u16 addr = emu_read_mem_word(emu, addr0);
    sprintf(log_buf, "JMP_IND: 0x%04x", addr);
    emu->cpu.pc = addr;
    emu->cycles += 5;
  } break;

    // JSR
  case OPCODE_JSR_ABS: {
    const u16 jmp_addr = fetch_word(emu);
    const u16 ret_addr = emu->cpu.pc;
    emu->cpu.sp++;
    emu->mem[emu->cpu.sp] = cpu_stat_get_byte(&emu->cpu);
    emu->cpu.sp++;
    emu->mem[emu->cpu.sp] = ret_addr >> 8;
    emu->cpu.sp++;
    emu->mem[emu->cpu.sp] = ret_addr & 0x00FF;
    sprintf(log_buf, "JSR_ABS: 0x%04x", jmp_addr);
    emu->cpu.pc = jmp_addr;
    emu->cycles += 6;
  } break;

    // NOP
  case OPCODE_NOP: {
  } break;

    // LDA
  case OPCODE_LDA_IM: {
    const u8 data = fetch_byte(emu);
    emu->cpu.a = data;
    set_flags_a(emu);
    emu->cycles += 2;
  } break;
  case OPCODE_LDA_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    emu->cpu.a = emu_read_mem_byte(emu, addr);
    set_flags_a(emu);
    emu->cycles += 3;
  } break;
  case OPCODE_LDA_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    emu->cpu.a = emu_read_mem_byte(emu, addr);
    set_flags_a(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDA_ABS: {
    u16 addr = fetch_word(emu);
    emu->cpu.a = emu_read_mem_byte(emu, addr);
    set_flags_a(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDA_ABSX: {
    const __auto_type result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->cpu.a = emu_read_mem_byte(emu, result.addr);
    set_flags_a(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDA_ABSY: {
    const __auto_type result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->cpu.a = emu_read_mem_byte(emu, result.addr);
    set_flags_a(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDA_INDX: {
    const u16 addr = fetch_addr_indx(emu);
    emu->cpu.a = emu_read_mem_byte(emu, addr);
    set_flags_a(emu);
    emu->cycles += 6;
  } break;
  case OPCODE_LDA_INDY: {
    const __auto_type result = fetch_addr_indy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->cpu.a = emu_read_mem_byte(emu, result.addr);
    set_flags_a(emu);
    emu->cycles += 5;
  } break;

    // LDX
  case OPCODE_LDX_IM: {
    u8 data = fetch_byte(emu);
    emu->cpu.x = data;
    set_flags_x(emu);
    emu->cycles += 2;
  } break;
  case OPCODE_LDX_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    emu->cpu.x = emu_read_mem_byte(emu, addr);
    set_flags_x(emu);
    emu->cycles += 3;
  } break;
  case OPCODE_LDX_ZPY: {
    const u16 addr = fetch_addr_zpy(emu);
    emu->cpu.x = emu_read_mem_byte(emu, addr);
    set_flags_x(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDX_ABS: {
    u16 addr = fetch_word(emu);
    emu->cpu.x = emu_read_mem_byte(emu, addr);
    set_flags_x(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDX_ABSY: {
    const __auto_type result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->cpu.x = emu_read_mem_byte(emu, result.addr);
    set_flags_x(emu);
    emu->cycles += 4;
  } break;

    // LDY
  case OPCODE_LDY_IM: {
    u8 data = fetch_byte(emu);
    emu->cpu.y = data;
    set_flags_y(emu);
    emu->cycles += 2;
  } break;
  case OPCODE_LDY_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    emu->cpu.y = emu_read_mem_byte(emu, addr);
    set_flags_y(emu);
    emu->cycles += 3;
  } break;
  case OPCODE_LDY_ZPY: {
    const u16 addr = fetch_addr_zpy(emu);
    emu->cpu.y = emu_read_mem_byte(emu, addr);
    set_flags_y(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDY_ABS: {
    u16 addr = fetch_word(emu);
    emu->cpu.y = emu_read_mem_byte(emu, addr);
    set_flags_y(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDY_ABSY: {
    const __auto_type result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->cpu.y = emu_read_mem_byte(emu, result.addr);
    set_flags_y(emu);
    emu->cycles += 4;
  } break;

    // PHA
  case OPCODE_PHA: {
    emu->cpu.sp++;
    emu->mem[emu->cpu.sp] = emu->cpu.a;
    emu->cycles += 3;
    if (emu->cpu.sp > 0x01FF) {
      sprintf(log_buf, "Stack overflowed");
      emu->is_running = false;
    }
  } break;

    // PHP
  case OPCODE_PHP: {
    emu->cpu.sp++;
    emu->mem[emu->cpu.sp] = cpu_stat_get_byte(&emu->cpu);
    emu->cycles += 3;
    if (emu->cpu.sp > 0x01FF) {
      sprintf(log_buf, "Stack overflowed");
      emu->is_running = false;
    }
  } break;

    // PLA
  case OPCODE_PLA: {
    emu->cpu.a = emu_read_mem_byte(emu, emu->cpu.sp);
    emu->cpu.sp--;
    emu->cycles += 4;
    if (emu->cpu.sp < 0x100) {
      sprintf(log_buf, "Stack underflowed");
      emu->is_running = false;
    }
  } break;

    // PLP
  case OPCODE_PLP: {
    u8 stat = emu_read_mem_byte(emu, emu->cpu.sp);
    cpu_set_stat_from_byte(&emu->cpu, stat);
    emu->cpu.sp--;
    emu->cycles += 4;
    if (emu->cpu.sp < 0x100) {
      sprintf(log_buf, "Stack underflowed");
      emu->is_running = false;
    }
  } break;

    // STA
  case OPCODE_STA_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    emu->mem[addr] = emu->cpu.a;
    emu->cycles += 3;
  } break;
  case OPCODE_STA_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    emu->mem[addr] = emu->cpu.a;
    emu->cycles += 4;
  } break;
  case OPCODE_STA_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    emu->mem[addr] = emu->cpu.a;
    emu->cycles += 4;
  } break;
  case OPCODE_STA_ABSX: {
    const __auto_type result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->mem[result.addr] = emu->cpu.a;
    emu->cycles += 5;
  } break;
  case OPCODE_STA_ABSY: {
    const __auto_type result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->mem[result.addr] = emu->cpu.a;
    emu->cycles += 5;
  } break;
  case OPCODE_STA_INDX: {
    const u16 addr = fetch_addr_indx(emu);
    emu->mem[addr] = emu->cpu.a;
    emu->cycles += 6;
  } break;
  case OPCODE_STA_INDY: {
    const __auto_type result = fetch_addr_indy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->mem[result.addr] = emu->cpu.a;
    emu->cycles += 6;
  } break;

    // STX
  case OPCODE_STX_ZP: {
    u16 addr = fetch_byte(emu);
    emu->mem[addr] = emu->cpu.x;
    emu->cycles += 3;
  } break;
  case OPCODE_STX_ZPY: {
    u16 addr = fetch_byte(emu) + emu->cpu.y;
    emu->mem[addr] = emu->cpu.x;
    emu->cycles += 4;
  } break;
  case OPCODE_STX_ABS: {
    u16 addr = fetch_word(emu);
    emu->mem[addr] = emu->cpu.x;
    emu->cycles += 4;
  } break;

    // STY
  case OPCODE_STY_ZP: {
    u16 addr = fetch_byte(emu);
    emu->mem[addr] = emu->cpu.y;
    emu->cycles += 3;
  } break;
  case OPCODE_STY_ZPX: {
    u16 addr = fetch_byte(emu) + emu->cpu.x;
    emu->mem[addr] = emu->cpu.y;
    emu->cycles += 4;
  } break;
  case OPCODE_STY_ABS: {
    u16 addr = fetch_word(emu);
    emu->mem[addr] = emu->cpu.y;
    emu->cycles += 4;
  } break;

    // TAX
  case OPCODE_TAX: {
    emu->cpu.x = emu->cpu.a;
    set_flags_x(emu);
    emu->cycles += 2;
  } break;

    // TAY
  case OPCODE_TAY: {
    emu->cpu.y = emu->cpu.a;
    set_flags_y(emu);
    emu->cycles += 2;
  } break;

    // TSX
  case OPCODE_TSX: {
    emu->cpu.x = (u8)emu->cpu.sp;
    set_flags_x(emu);
    emu->cycles += 2;
  } break;

    // TXA
  case OPCODE_TXA: {
    emu->cpu.a = emu->cpu.x;
    set_flags_a(emu);
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
    set_flags_y(emu);
    emu->cycles += 2;
  } break;

  default: {
    emu->is_running = false;
  } break;
  }
  PRINT_STAT("", opcode);
  return;
}
