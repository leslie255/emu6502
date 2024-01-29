#include "emu6502.h"
#include "calc.h"

#include <arm/endian.h>
#include <ncurses.h>
#include <stdarg.h>
#include <sys/_endian.h>

#define LPRINTF(EMU, ...)                                                      \
  if (EMU->debug_output) {                                                     \
    const usize i = strlen(emu->log_buf);                                      \
    char *buff = &emu->log_buf[i];                                             \
    sprintf(buff, __VA_ARGS__);                                                \
  }

void mem_init(u8 *mem) { bzero(mem, MEM_SIZE); }

void cpu_reset_sr(CPU *cpu) { cpu->sr.byte = 0; }

void cpu_reset(CPU *cpu) {
  cpu->pc = 0xFFFC;
  cpu->sp = 0xFF;
  cpu_reset_sr(cpu);
}

static inline char zero_or_one(const u8 x) { return (x == 0) ? '0' : '1'; }

void cpu_debug_print(const CPU *cpu) {
  printw("REG\tHEX\tDEC(u)\tDEC(i)\n"
         "PC:\t%04X\t%u\t%d\n"
         "SP:\t%02X\t%u\t%d\n"
         "A:\t%02X\t%u\t%d\n"
         "X:\t%02X\t%u\t%d\n"
         "Y:\t%02X\t%u\t%d\n"
         "C   Z   I   D   B   V   N \n"
         "%c   %c   %c   %c   %c   %c   %c\n",
         cpu->pc, cpu->pc, (i16)cpu->pc, cpu->sp, cpu->sp, (i8)cpu->sp, cpu->a,
         cpu->a, (i8)cpu->a, cpu->x, cpu->x, (i8)cpu->x, cpu->y, cpu->y,
         (i8)cpu->y, zero_or_one(cpu->sr.bits.c), zero_or_one(cpu->sr.bits.z),
         zero_or_one(cpu->sr.bits.i), zero_or_one(cpu->sr.bits.d),
         zero_or_one(cpu->sr.bits.b), zero_or_one(cpu->sr.bits.v),
         zero_or_one(cpu->sr.bits.n));
}

void emu_init(Emulator *emu, bool debug_output) {
  cpu_reset(&emu->cpu);
  mem_init(emu->mem);
  emu->cycles = 0;
  emu->is_running = true;
  emu->debug_output = debug_output;
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

  data = ntohs(data);

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
  const u16 addr0 = fetch_byte(emu) + emu->cpu.x;
  return emu_read_mem_word(emu, addr0);
}

// get an address on the position of PC by addressing mode Zero Page Y
static inline u16 fetch_addr_zpy(Emulator *emu) {
  const u16 addr0 = fetch_byte(emu);
  return emu_read_mem_word(emu, addr0 + emu->cpu.y);
}

// get an address on the position of PC by addressing mode Absolute
static inline u16 fetch_addr_abs(Emulator *emu) { return fetch_word(emu); }

// get an address on the position of PC by addressing mode Absolute,X
static inline struct addr_fetch_result fetch_addr_absx(Emulator *emu) {
  const u16 addr0 = fetch_word(emu);
  const u16 addr1 = addr0 + emu->cpu.x;
  const bool page_crossed = ((addr0 & 0xFF00) != (addr1 & 0xFF00));
  return (struct addr_fetch_result){addr1, page_crossed};
}

// get an address on the position of PC by addressing mode Absolute,Y
static inline struct addr_fetch_result fetch_addr_absy(Emulator *emu) {
  const u16 addr0 = fetch_word(emu);
  const u16 addr1 = addr0 + emu->cpu.y;
  const bool page_crossed = ((addr0 & 0xFF00) != (addr1 & 0xFF00));
  return (struct addr_fetch_result){addr1, page_crossed};
}

// get an address on the position of PC by addressing mode (Indirect,X)
static inline u16 fetch_addr_indx(Emulator *emu) {
  const u16 addr0 = fetch_word(emu) + emu->cpu.x;
  const u16 addr1 = emu_read_mem_word(emu, addr0);
  return addr1;
}

// get an address on the position of PC by addressing mode (Indirect),Y
static inline struct addr_fetch_result fetch_addr_indy(Emulator *emu) {
  const u16 addr0 = fetch_word(emu);
  const u16 addr1 = emu_read_mem_word(emu, addr0) + emu->cpu.y;
  const bool page_crossed = ((addr0 & 0xFF00) != (addr1 & 0xFF00));
  return (struct addr_fetch_result){addr1, page_crossed};
}

// update flags in the CPU according to a byte
static inline void set_nz_flags(Emulator *emu, const u8 byte) {
  emu->cpu.sr.bits.z = (byte == 0);
  emu->cpu.sr.bits.n = (byte & 0b10000000) >> 7;
}

// update flags in the CPU according to register A
static inline void set_nz_flags_a(Emulator *emu) {
  set_nz_flags(emu, emu->cpu.a);
}

// update flags in the CPU according to register X
static inline void set_nz_flags_x(Emulator *emu) {
  set_nz_flags(emu, emu->cpu.x);
}

// update flags in the CPU according to register Y
static inline void set_nz_flags_y(Emulator *emu) {
  set_nz_flags(emu, emu->cpu.y);
}

static inline void cmp(Emulator *emu, const u8 lhs, const u8 rhs) {
  LPRINTF(emu, "cmp: 0x%02X vs 0x%02X\n", lhs, rhs);
  const u8 sub_result = (lhs - rhs);
  set_nz_flags(emu, sub_result);
  emu->cpu.sr.bits.c = (lhs >= rhs);
}

static inline void cmp_a(Emulator *emu, const u8 rhs) {
  cmp(emu, emu->cpu.a, rhs);
}

static inline void cmp_x(Emulator *emu, const u8 rhs) {
  cmp(emu, emu->cpu.x, rhs);
}

static inline void cmp_y(Emulator *emu, const u8 rhs) {
  cmp(emu, emu->cpu.y, rhs);
}

static inline void op_adc(Emulator *emu, const u8 rhs) {
  const auto f = emu->cpu.sr.bits.d ? carrying_bcd_add_u8 : carrying_add_u8;
  const auto sum_carry = f(emu->cpu.a, rhs, emu->cpu.sr.bits.c);
  LPRINTF(
      emu,
      "%02X(a) + %02X(m) + %01X(c) = %02X(s) ... %1X(c)\ndecimal mode: %s\n",
      emu->cpu.a, rhs, emu->cpu.sr.bits.c, sum_carry.result, sum_carry.carry,
      emu->cpu.sr.bits.d ? "on" : "off");
  set_nz_flags_a(emu);
  emu->cpu.sr.bits.c = sum_carry.carry;
  emu->cpu.sr.bits.v = sum_carry.carry;
  emu->cpu.a = sum_carry.result;
}

static inline void op_sbc(Emulator *emu, const u8 rhs) {
  const auto f = emu->cpu.sr.bits.d ? carrying_bcd_sub_u8 : carrying_sub_u8;
  const auto dif_carry = f(emu->cpu.a, rhs, emu->cpu.sr.bits.c);
  LPRINTF(
      emu,
      "%02X(a) - %02X(m) - %01X(c) = %02X(s) ... %1X(c)\ndecimal mode: %s\n",
      emu->cpu.a, rhs, emu->cpu.sr.bits.c, dif_carry.result, dif_carry.carry,
      emu->cpu.sr.bits.d ? "on" : "off");
  set_nz_flags_a(emu);
  emu->cpu.sr.bits.c = dif_carry.carry;
  emu->cpu.sr.bits.v = dif_carry.carry;
  emu->cpu.a = dif_carry.result;
}

static inline void op_and(Emulator *emu, const u8 rhs) {
  emu->cpu.a &= rhs;
  set_nz_flags_a(emu);
}

static inline void op_ora(Emulator *emu, const u8 rhs) {
  emu->cpu.a |= rhs;
  set_nz_flags_a(emu);
}

static inline void op_eor(Emulator *emu, const u8 rhs) {
  emu->cpu.a ^= rhs;
  set_nz_flags_a(emu);
}

static inline void op_bit(Emulator *emu, const u8 x) {
  cpu_reset_sr(&emu->cpu);
  emu->cpu.sr.bits.n = (x & 0b10000000) >> 7;
  emu->cpu.sr.bits.v = (x & 0b01000000) >> 6;
  emu->cpu.sr.bits.z = ((x & emu->cpu.a) == 0);
}

// Performs ASL operation
// Returns the result value.
static inline u8 op_asl(Emulator *emu, const u8 x) {
  const u8 result = (u8)(x << 1);
  set_nz_flags(emu, result);
  emu->cpu.sr.bits.c = ((x & 0b10000000) != 0);
  return result;
}

// Performs LSR operation
// Returns the result value.
static inline u8 op_lsr(Emulator *emu, const u8 x) {
  const u8 result = (x >> 1);
  cpu_reset_sr(&emu->cpu);
  emu->cpu.sr.bits.n = false;
  emu->cpu.sr.bits.z = (result == 0);
  emu->cpu.sr.bits.c = ((x & 0b00000001) != 0);
  return result;
}

// Performs ROL operation
// Returns the result value.
static inline u8 op_rol(Emulator *emu, const u8 x) {
  const u8 result = (u8)(x << 1) | emu->cpu.sr.bits.c;
  set_nz_flags(emu, result);
  emu->cpu.sr.bits.c = ((x & 0b10000000) != 0);
  return result;
}

// Performs ROR operation
// Returns the result value.
static inline u8 op_ror(Emulator *emu, const u8 x) {
  const u8 result = (x >> 1) | (u8)(emu->cpu.sr.bits.c << 7);
  set_nz_flags(emu, result);
  emu->cpu.sr.bits.c = ((x & 0b00000001) != 0);
  return result;
}

// Performs a branch operation by relative addressing mode.
// Will fetch a byte forward.
// Also increments cycle by 1 or 2.
// Still requires cycle to increment by 2 outside of this function.
// Returns target address.
static inline u16 branch_rel(Emulator *emu) {
  const u16 current = emu->cpu.pc - 1;
  const u8 addr_rel = fetch_byte(emu);
  const u16 target_addr = ((addr_rel & 0b10000000) == 0)
                              // positive
                              ? current + addr_rel
                              // negative
                              : current - (0xFF - addr_rel + 1);
  emu->cycles++;
  if ((emu->cpu.pc & 0xFF00) != (target_addr & 0xFF00)) {
    emu->cycles++;
  }
  emu->cpu.pc = target_addr;
  return target_addr;
}

static inline void stack_push(Emulator *emu, const u8 byte) {
  emu->mem[0x0100 | (u16)emu->cpu.sp] = byte;
  emu->cpu.sp--;
}

static inline u8 stack_pull(Emulator *emu) {
  emu->cpu.sp++;
  const u16 p = 0x0100 | (u16)emu->cpu.sp;
  return emu->mem[p];
}

// Push the current values of SR and PC onto the stack
// Used for function call
static inline void push_callstack(Emulator *emu) {
  const u16 pc = emu->cpu.pc;
  LPRINTF(emu, "PC pushed: %04X\n", pc);
  stack_push(emu, pc & 0x00FF);
  stack_push(emu, pc >> 8);
  stack_push(emu, emu->cpu.sr.byte);
}

// Pull the value of SR and PC from the stack.
// Used for function return
static inline void pull_callstack(Emulator *emu) {
  emu->cpu.sr.byte = stack_pull(emu);
  u16 pc = (u16)(stack_pull(emu) << 8);
  pc |= stack_pull(emu);
  emu->cpu.pc = pc;
  LPRINTF(emu, "PC pulled: %04X\n", pc);
}

void emu_print_stack(const Emulator *emu) {
  printw("\t_0 _1 _2 _3 _4 _5 _6 _7 _8 _9 _A _B _C _D _E _F\n");
  for (usize i = STACK_FLOOR; i <= STACK_LIMIT; i += 16) {
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

  data = htons(data);

  return data;
}

void emu_tick(Emulator *emu) {
#define PRINT_STAT(...)                                                        \
  if (emu->debug_output) {                                                     \
    printw(""__VA_ARGS__);                                                     \
    printw("Opcode:\t0x%02X\n"                                                 \
           "Addr:\t0x%04X\n"                                                   \
           "Cycles:\t%llu\n"                                                   \
           "CPU status:\n",                                                    \
           opcode, emu->cpu.pc - 1, emu->cycles);                              \
    cpu_debug_print(&emu->cpu);                                                \
    printw("Stack:\n");                                                        \
    emu_print_stack(emu);                                                      \
    printw("log: -----------\n%s----------\n", emu->log_buf);                  \
    bzero(&emu->log_buf, LOG_BUF_SIZE);                                        \
  }

  const u8 opcode = fetch_byte(emu);

  switch (opcode) {
  // ADC
  case OPCODE_ADC_IM: {
    const u8 rhs = fetch_byte(emu);
    op_adc(emu, rhs);
    emu->cycles += 2;
  } break;
  case OPCODE_ADC_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    op_adc(emu, emu->mem[addr]);
    emu->cycles += 3;
  } break;
  case OPCODE_ADC_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    op_adc(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_ADC_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    op_adc(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_ADC_ABSX: {
    const auto result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_adc(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_ADC_ABSY: {
    const auto result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_adc(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_ADC_INDX: {
    const u16 addr = fetch_addr_indx(emu);
    op_adc(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_ADC_INDY: {
    const auto result = fetch_addr_indy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_adc(emu, emu->mem[result.addr]);
    emu->cycles += 5;
  } break;

  // AND
  case OPCODE_AND_IM: {
    const u8 rhs = fetch_byte(emu);
    op_and(emu, rhs);
    emu->cycles += 2;
  } break;
  case OPCODE_AND_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    op_and(emu, emu->mem[addr]);
    emu->cycles += 3;
  } break;
  case OPCODE_AND_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    op_and(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_AND_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    op_and(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_AND_ABSX: {
    const auto result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_and(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_AND_ABSY: {
    const auto result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_and(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_AND_INDX: {
    const u16 addr = fetch_addr_indx(emu);
    op_and(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_AND_INDY: {
    const auto result = fetch_addr_indy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_and(emu, emu->mem[result.addr]);
    emu->cycles += 5;
  } break;

    // ASL
  case OPCODE_ASL_A: {
    emu->cpu.a = op_asl(emu, emu->cpu.a);
    emu->cycles += 2;
  } break;
  case OPCODE_ASL_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    emu->mem[addr] = op_asl(emu, emu->mem[addr]);
    emu->cycles += 5;
  } break;
  case OPCODE_ASL_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    emu->mem[addr] = op_asl(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_ASL_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    emu->mem[addr] = op_asl(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_ASL_ABSX: {
    const u16 addr = fetch_addr_absx(emu).addr;
    emu->mem[addr] = op_asl(emu, emu->mem[addr]);
    emu->cycles += 7;
  } break;

    // BCC
  case OPCODE_BCC_REL: {
    emu->cycles += 2;
    if (emu->cpu.sr.bits.c == false) {
      const u16 target_addr = branch_rel(emu);
      LPRINTF(emu, "BCC: 0x%04X\n", target_addr);
    } else {
      LPRINTF(emu, "BCC: not jumped\n");
    }
  } break;

    // BCS
  case OPCODE_BCS_REL: {
    emu->cycles += 2;
    if (emu->cpu.sr.bits.c == true) {
      const u16 target_addr = branch_rel(emu);
      LPRINTF(emu, "BCS: 0x%04X\n", target_addr);
    } else {
      emu->cpu.pc++;
      LPRINTF(emu, "BCS: not jumped\n");
    }
  } break;

    // BEQ
  case OPCODE_BEQ_REL: {
    emu->cycles += 2;
    if (emu->cpu.sr.bits.z == true) {
      const u16 target_addr = branch_rel(emu);
      LPRINTF(emu, "BEQ: 0x%04X\n", target_addr);
    } else {
      LPRINTF(emu, "BEQ: not jumped\n");
    }
  } break;

    // BIT
  case OPCODE_BIT_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    op_bit(emu, emu->mem[addr]);
    emu->cycles += 3;
  } break;

    // BIT
  case OPCODE_BIT_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    op_bit(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;

    // BMI
  case OPCODE_BMI_REL: {
    emu->cycles += 2;
    if (emu->cpu.sr.bits.n == true) {
      const u16 target_addr = branch_rel(emu);
      LPRINTF(emu, "BMI: 0x%04X\n", target_addr);
    } else {
      LPRINTF(emu, "BMI: not jumped\n");
    }
  } break;

    // BNE
  case OPCODE_BNE_REL: {
    emu->cycles += 2;
    if (emu->cpu.sr.bits.z == false) {
      const u16 target_addr = branch_rel(emu);
      LPRINTF(emu, "BNE: 0x%04X\n", target_addr);
    } else {
      LPRINTF(emu, "BNE: not jumped\n");
    }
  } break;

    // BPL
  case OPCODE_BPL_REL: {
    emu->cycles += 2;
    if (emu->cpu.sr.bits.n == false) {
      const u16 target_addr = branch_rel(emu);
      LPRINTF(emu, "BPL: 0x%04X\n", target_addr);
    } else {
      LPRINTF(emu, "BPL: not jumped\n");
    }
  } break;

    // BRK
  case OPCODE_BRK: {
    LPRINTF(emu, "Interrupted (BRK)\n");
    cpu_reset_sr(&emu->cpu);
    push_callstack(emu);
    emu->cpu.sr.bits.i = true;
    emu->is_running = false;
  } break;

    // BVC
  case OPCODE_BVC_REL: {
    emu->cycles += 2;
    if (emu->cpu.sr.bits.v == false) {
      const u16 target_addr = branch_rel(emu);
      LPRINTF(emu, "BVC: 0x%04X\n", target_addr);
    } else {
      LPRINTF(emu, "BVC: not jumped\n");
    }
  } break;

    // BVS
  case OPCODE_BVS_REL: {
    emu->cycles += 2;
    if (emu->cpu.sr.bits.v == true) {
      const u16 target_addr = branch_rel(emu);
      LPRINTF(emu, "BVS: 0x%04X\n", target_addr);
    } else {
      LPRINTF(emu, "BVS: not jumped\n");
    }
  } break;

    // CLC
  case OPCODE_CLC: {
    emu->cpu.sr.bits.c = false;
    emu->cycles += 2;
  } break;

    // CLD
  case OPCODE_CLD: {
    emu->cpu.sr.bits.d = false;
    emu->cycles += 2;
  } break;

    // CLI
  case OPCODE_CLI: {
    emu->cpu.sr.bits.i = false;
    emu->cycles += 2;
  } break;

    // CLV
  case OPCODE_CLV: {
    emu->cpu.sr.bits.v = false;
    emu->cycles += 2;
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
    const auto result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cpu.pc++;
    }
    cmp_a(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_CMP_ABSY: {
    const auto result = fetch_addr_absy(emu);
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
    const auto result = fetch_addr_indy(emu);
    if (result.page_crossed) {
      emu->cpu.pc++;
    }
    cmp_a(emu, emu->mem[result.addr]);
    emu->cycles += 5;
  } break;

    // CPX
  case OPCODE_CPX_IM: {
    const u8 byte = fetch_byte(emu);
    cmp_x(emu, byte);
    emu->cycles += 2;
  } break;
  case OPCODE_CPX_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    cmp_x(emu, emu->mem[addr]);
    emu->cycles += 3;
  } break;
  case OPCODE_CPX_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    cmp_x(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;

    // CPY
  case OPCODE_CPY_IM: {
    const u8 byte = fetch_byte(emu);
    cmp_y(emu, byte);
    emu->cycles += 2;
  } break;
  case OPCODE_CPY_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    cmp_y(emu, emu->mem[addr]);
    emu->cycles += 3;
  } break;
  case OPCODE_CPY_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    cmp_y(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;

    // DEC
  case OPCODE_DEC_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)--;
    set_nz_flags(emu, *byte);
    emu->cycles += 3;
  } break;
  case OPCODE_DEC_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)--;
    set_nz_flags(emu, *byte);
    emu->cycles += 3;
  } break;
  case OPCODE_DEC_ABS: {
    const u16 addr = fetch_word(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)--;
    set_nz_flags(emu, *byte);
    emu->cycles += 4;
  } break;
  case OPCODE_DEC_ABSX: {
    const auto result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    u8 *byte = &emu->mem[result.addr];
    (*byte)--;
    set_nz_flags(emu, *byte);
    emu->cycles += 4;
  } break;

    // INX
  case OPCODE_INX: {
    emu->cpu.x--;
    set_nz_flags_x(emu);
    emu->cycles += 2;
  } break;

    // INY
  case OPCODE_INY: {
    emu->cpu.y--;
    set_nz_flags_y(emu);
    emu->cycles += 2;
  } break;

    // INC
  case OPCODE_INC_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)++;
    set_nz_flags(emu, *byte);
    emu->cycles += 3;
  } break;
  case OPCODE_INC_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)++;
    set_nz_flags(emu, *byte);
    emu->cycles += 3;
  } break;
  case OPCODE_INC_ABS: {
    const u16 addr = fetch_word(emu);
    u8 *byte = &emu->mem[addr];
    (*byte)++;
    set_nz_flags(emu, *byte);
    emu->cycles += 4;
  } break;
  case OPCODE_INC_ABSX: {
    const auto result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    u8 *byte = &emu->mem[result.addr];
    (*byte)++;
    set_nz_flags(emu, *byte);
    emu->cycles += 4;
  } break;

    // DEX
  case OPCODE_DEX: {
    emu->cpu.x++;
    set_nz_flags_x(emu);
    emu->cycles += 2;
  } break;

    // DEY
  case OPCODE_DEY: {
    emu->cpu.y++;
    set_nz_flags_y(emu);
    emu->cycles += 2;
  } break;

    // EOR
  case OPCODE_EOR_IM: {
    const u8 rhs = fetch_byte(emu);
    op_eor(emu, rhs);
    emu->cycles += 2;
  } break;
  case OPCODE_EOR_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    op_eor(emu, emu->mem[addr]);
    emu->cycles += 3;
  } break;
  case OPCODE_EOR_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    op_eor(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_EOR_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    op_eor(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_EOR_ABSX: {
    const auto result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_eor(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_EOR_ABSY: {
    const auto result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_eor(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_EOR_INDX: {
    const u16 addr = fetch_addr_indx(emu);
    op_eor(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_EOR_INDY: {
    const auto result = fetch_addr_indy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_eor(emu, emu->mem[result.addr]);
    emu->cycles += 5;
  } break;

    // JMP
  case OPCODE_JMP_ABS: {
    u16 addr = fetch_word(emu);
    LPRINTF(emu, "JMP_ABS: 0x%04x\n", addr);
    emu->cpu.pc = addr;
    emu->cycles += 3;
  } break;
  case OPCODE_JMP_IND: {
    u16 addr0 = fetch_word(emu);
    u16 addr = emu_read_mem_word(emu, addr0);
    LPRINTF(emu, "JMP_IND: 0x%04x\n", addr);
    emu->cpu.pc = addr;
    emu->cycles += 5;
  } break;

    // JSR
  case OPCODE_JSR_ABS: {
    const u16 jmp_addr = fetch_word(emu);
    push_callstack(emu);
    LPRINTF(emu, "JSR_ABS: 0x%04x\n", jmp_addr);
    emu->cpu.pc = jmp_addr;
    emu->cycles += 6;
  } break;

    // NOP
  case OPCODE_NOP: {
    emu->cycles += 2;
  } break;

    // ORA
  case OPCODE_ORA_IM: {
    const u8 rhs = fetch_byte(emu);
    op_ora(emu, rhs);
    emu->cycles += 2;
  } break;
  case OPCODE_ORA_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    op_ora(emu, emu->mem[addr]);
    emu->cycles += 3;
  } break;
  case OPCODE_ORA_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    op_ora(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_ORA_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    op_ora(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_ORA_ABSX: {
    const auto result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_ora(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_ORA_ABSY: {
    const auto result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_ora(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_ORA_INDX: {
    const u16 addr = fetch_addr_indx(emu);
    op_ora(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_ORA_INDY: {
    const auto result = fetch_addr_indy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_ora(emu, emu->mem[result.addr]);
    emu->cycles += 5;
  } break;

    // LDA
  case OPCODE_LDA_IM: {
    const u8 data = fetch_byte(emu);
    emu->cpu.a = data;
    set_nz_flags_a(emu);
    emu->cycles += 2;
  } break;
  case OPCODE_LDA_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    emu->cpu.a = emu->mem[addr];
    set_nz_flags_a(emu);
    emu->cycles += 3;
  } break;
  case OPCODE_LDA_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    emu->cpu.a = emu->mem[addr];
    set_nz_flags_a(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDA_ABS: {
    u16 addr = fetch_word(emu);
    emu->cpu.a = emu->mem[addr];
    set_nz_flags_a(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDA_ABSX: {
    const auto result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->cpu.a = emu->mem[result.addr];
    set_nz_flags_a(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDA_ABSY: {
    const auto result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->cpu.a = emu->mem[result.addr];
    set_nz_flags_a(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDA_INDX: {
    const u16 addr = fetch_addr_indx(emu);
    emu->cpu.a = emu->mem[addr];
    set_nz_flags_a(emu);
    emu->cycles += 6;
  } break;
  case OPCODE_LDA_INDY: {
    const auto result = fetch_addr_indy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->cpu.a = emu->mem[result.addr];
    set_nz_flags_a(emu);
    emu->cycles += 5;
  } break;

    // LDX
  case OPCODE_LDX_IM: {
    u8 data = fetch_byte(emu);
    emu->cpu.x = data;
    set_nz_flags_x(emu);
    emu->cycles += 2;
  } break;
  case OPCODE_LDX_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    emu->cpu.x = emu->mem[addr];
    set_nz_flags_x(emu);
    emu->cycles += 3;
  } break;
  case OPCODE_LDX_ZPY: {
    const u16 addr = fetch_addr_zpy(emu);
    emu->cpu.x = emu->mem[addr];
    set_nz_flags_x(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDX_ABS: {
    u16 addr = fetch_word(emu);
    emu->cpu.x = emu->mem[addr];
    set_nz_flags_x(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDX_ABSY: {
    const auto result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->cpu.x = emu->mem[result.addr];
    set_nz_flags_x(emu);
    emu->cycles += 4;
  } break;

    // LDY
  case OPCODE_LDY_IM: {
    u8 data = fetch_byte(emu);
    emu->cpu.y = data;
    set_nz_flags_y(emu);
    emu->cycles += 2;
  } break;
  case OPCODE_LDY_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    emu->cpu.y = emu->mem[addr];
    set_nz_flags_y(emu);
    emu->cycles += 3;
  } break;
  case OPCODE_LDY_ZPY: {
    const u16 addr = fetch_addr_zpy(emu);
    emu->cpu.y = emu->mem[addr];
    set_nz_flags_y(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDY_ABS: {
    u16 addr = fetch_word(emu);
    emu->cpu.y = emu->mem[addr];
    set_nz_flags_y(emu);
    emu->cycles += 4;
  } break;
  case OPCODE_LDY_ABSY: {
    const auto result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->cpu.y = emu->mem[result.addr];
    set_nz_flags_y(emu);
    emu->cycles += 4;
  } break;

    // LSR
  case OPCODE_LSR_A: {
    emu->cpu.a = op_lsr(emu, emu->cpu.a);
    emu->cycles += 2;
  } break;
  case OPCODE_LSR_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    emu->mem[addr] = op_lsr(emu, emu->mem[addr]);
    emu->cycles += 5;
  } break;
  case OPCODE_LSR_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    emu->mem[addr] = op_lsr(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_LSR_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    emu->mem[addr] = op_lsr(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_LSR_ABSX: {
    const u16 addr = fetch_addr_absx(emu).addr;
    emu->mem[addr] = op_lsr(emu, emu->mem[addr]);
    emu->cycles += 7;
  } break;

    // PHA
  case OPCODE_PHA: {
    stack_push(emu, emu->cpu.a);
    emu->cycles += 3;
  } break;

    // PHP
  case OPCODE_PHP: {
    stack_push(emu, emu->cpu.sr.byte);
    emu->cycles += 3;
  } break;

    // PLA
  case OPCODE_PLA: {
    emu->cpu.a = stack_pull(emu);
  } break;

    // PLP
  case OPCODE_PLP: {
    const u8 sr = stack_pull(emu);
    emu->cpu.sr.byte = sr;
    emu->cycles += 4;
  } break;

    // ROL
  case OPCODE_ROL_A: {
    emu->cpu.a = op_rol(emu, emu->cpu.a);
    emu->cycles += 2;
  } break;
  case OPCODE_ROL_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    emu->mem[addr] = op_rol(emu, emu->mem[addr]);
    emu->cycles += 5;
  } break;
  case OPCODE_ROL_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    emu->mem[addr] = op_rol(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_ROL_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    emu->mem[addr] = op_rol(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_ROL_ABSX: {
    const u16 addr = fetch_addr_absx(emu).addr;
    emu->mem[addr] = op_rol(emu, emu->mem[addr]);
    emu->cycles += 7;
  } break;

    // ROR
  case OPCODE_ROR_A: {
    emu->cpu.a = op_ror(emu, emu->cpu.a);
    emu->cycles += 2;
  } break;
  case OPCODE_ROR_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    emu->mem[addr] = op_ror(emu, emu->mem[addr]);
    emu->cycles += 5;
  } break;
  case OPCODE_ROR_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    emu->mem[addr] = op_ror(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_ROR_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    emu->mem[addr] = op_ror(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_ROR_ABSX: {
    const u16 addr = fetch_addr_absx(emu).addr;
    emu->mem[addr] = op_ror(emu, emu->mem[addr]);
    emu->cycles += 7;
  } break;

    // RTI
  case OPCODE_RTI: {
    pull_callstack(emu);
    emu->cpu.sr.bits.i = false;
    emu->is_running = true;
  } break;

    // RTS
  case OPCODE_RTS: {
    pull_callstack(emu);
    emu->cycles += 6;
  } break;

  // SBC
  case OPCODE_SBC_IM: {
    const u8 rhs = fetch_byte(emu);
    op_sbc(emu, rhs);
    emu->cycles += 2;
  } break;
  case OPCODE_SBC_ZP: {
    const u16 addr = fetch_addr_zp(emu);
    op_sbc(emu, emu->mem[addr]);
    emu->cycles += 3;
  } break;
  case OPCODE_SBC_ZPX: {
    const u16 addr = fetch_addr_zpx(emu);
    op_sbc(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_SBC_ABS: {
    const u16 addr = fetch_addr_abs(emu);
    op_sbc(emu, emu->mem[addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_SBC_ABSX: {
    const auto result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_sbc(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_SBC_ABSY: {
    const auto result = fetch_addr_absy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_sbc(emu, emu->mem[result.addr]);
    emu->cycles += 4;
  } break;
  case OPCODE_SBC_INDX: {
    const u16 addr = fetch_addr_indx(emu);
    op_sbc(emu, emu->mem[addr]);
    emu->cycles += 6;
  } break;
  case OPCODE_SBC_INDY: {
    const auto result = fetch_addr_indy(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    op_sbc(emu, emu->mem[result.addr]);
    emu->cycles += 5;
  } break;

    // SEC
  case OPCODE_SEC: {
    emu->cpu.sr.bits.c = true;
    emu->cycles += 2;
  } break;

    // SED
  case OPCODE_SED: {
    emu->cpu.sr.bits.d = true;
    emu->cycles += 2;
  } break;

    // SEI
  case OPCODE_SEI: {
    emu->cpu.sr.bits.i = true;
    emu->cycles += 2;
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
    const auto result = fetch_addr_absx(emu);
    if (result.page_crossed) {
      emu->cycles++;
    }
    emu->mem[result.addr] = emu->cpu.a;
    emu->cycles += 5;
  } break;
  case OPCODE_STA_ABSY: {
    const auto result = fetch_addr_absy(emu);
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
    const auto result = fetch_addr_indy(emu);
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
    set_nz_flags_x(emu);
    emu->cycles += 2;
  } break;

    // TAY
  case OPCODE_TAY: {
    emu->cpu.y = emu->cpu.a;
    set_nz_flags_y(emu);
    emu->cycles += 2;
  } break;

    // TSX
  case OPCODE_TSX: {
    emu->cpu.x = (u8)emu->cpu.sp;
    set_nz_flags_x(emu);
    emu->cycles += 2;
  } break;

    // TXA
  case OPCODE_TXA: {
    emu->cpu.a = emu->cpu.x;
    set_nz_flags_a(emu);
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
    set_nz_flags_y(emu);
    emu->cycles += 2;
  } break;

  default: {
    emu->is_running = false;
    LPRINTF(emu, "Illegal opcode: 0x%02X\n", opcode);
  } break;
  }
  PRINT_STAT("", opcode);
  return;
}
