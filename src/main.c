#include "common.h"
#include "opcode.h"

// 64 kB
#define MEM_SIZE 65536

void mem_init(u8 *mem) { bzero(mem, MEM_SIZE); }

typedef struct {
  u16 pc;    // Program Counter
  u16 sp;    // Stack Pointer
  u8 a;      // Register A
  u8 x;      // Register X
  u8 y;      // Register Y
  u8 flag_c; // Carry Flag
  u8 flag_z; // Zero Flag
  u8 flag_i; // Interrupt Disable
  u8 flag_d; // Decimal Mode
  u8 flag_b; // Break Command
  u8 flag_v; // Overflow Flag
  u8 flag_n; // Negative Flag
} CPU;

void cpu_reset(CPU *cpu) {
  cpu->pc = 0xFFFC;
  cpu->sp = 0x0100;
  cpu->flag_d = 0;
  cpu->flag_c = 0;
  cpu->flag_z = 0;
  cpu->flag_i = 0;
  cpu->flag_d = 0;
  cpu->flag_b = 0;
  cpu->flag_v = 0;
  cpu->flag_n = 0;
}

void cpu_debug_print(CPU *cpu) {
  printf("\033[1;33mREG\tHEX\tDEC(u)\tDEC(i)\n\033[0m"
         "PC:\t%04X\t%u\t%d\n"
         "SP:\t%04X\t%u\t%d\n"
         "A:\t%02X\t%u\t%d\n"
         "X:\t%02X\t%u\t%d\n"
         "Y:\t%02X\t%u\t%d\n"
         "C:\t%02X\t%u\t%d\n"
         "Z:\t%02X\t%u\t%d\n"
         "I:\t%02X\t%u\t%d\n"
         "D:\t%02X\t%u\t%d\n"
         "B:\t%02X\t%u\t%d\n"
         "V:\t%02X\t%u\t%d\n"
         "N:\t%02X\t%u\t%d\n",
         cpu->pc, cpu->pc, (i16)cpu->pc, cpu->sp, cpu->sp, (i16)cpu->sp, cpu->a,
         cpu->a, (i8)cpu->a, cpu->x, cpu->x, (i8)cpu->x, cpu->y, cpu->y,
         (i8)cpu->y, cpu->flag_c, cpu->flag_c, (i8)cpu->flag_c, cpu->flag_z,
         cpu->flag_z, (i8)cpu->flag_z, cpu->flag_i, cpu->flag_i,
         (i8)cpu->flag_i, cpu->flag_d, cpu->flag_d, (i8)cpu->flag_d,
         cpu->flag_b, cpu->flag_b, (i8)cpu->flag_b, cpu->flag_v, cpu->flag_v,
         (i8)cpu->flag_v, cpu->flag_n, cpu->flag_n, (i8)cpu->flag_n);
}

typedef struct {
  CPU cpu;
  u8 mem[MEM_SIZE];
  u64 cycles_needed; // number of cycles needed to be executed
  u64 cycles_executed;
} Emulator;

void emu_init(Emulator *emu, u64 cycles) {
  cpu_reset(&emu->cpu);
  mem_init(emu->mem);
  emu->cycles_needed = cycles;
  emu->cycles_executed = 0;
}

// fetch 1 byte from memory on position of PC
u8 emu_fetch_byte(Emulator *emu) {
  u8 data = emu->mem[emu->cpu.pc];
  emu->cpu.pc++;
  return data;
}

// fetch 2 bytes from memory on position of PC
u16 emu_fetch_word(Emulator *emu) {
  // 6502 uses little endian
  u16 data = emu->mem[emu->cpu.pc];
  emu->cpu.pc++;
  data |= emu->mem[emu->cpu.pc] << 8;
  emu->cpu.pc++;

  if (BIG_ENDIAN) {
    data = ((data << 8) & 0xff00) | ((data >> 8) & 0x00ff);
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

u8 emu_read_mem_byte(Emulator *emu, u16 addr) { return emu->mem[addr]; }

u16 emu_read_mem_word(Emulator *emu, u16 addr) {
  // 6502 uses little endian
  u16 data = emu->mem[addr];
  data |= emu->mem[addr + 1] << 8;
  emu->cpu.pc++;

  if (BIG_ENDIAN) {
    data = ((data << 8) & 0xff00) | ((data >> 8) & 0x00ff);
  }

  return data;
}

void emu_execute(Emulator *emu) {

#define PRINT_STAT(...)                                                        \
  printf("\033[1;31m"__VA_ARGS__);                                             \
  printf("\033[0mOpcode:\t0x%04X\n"                                            \
         "Addr:\t0x%04X\n"                                                     \
         "Cycles:\t%llu\n"                                                     \
         "CPU status:\n",                                                      \
         opcode, emu->cpu.pc - 1, emu->cycles_executed);                       \
  cpu_debug_print(&emu->cpu);

  u8 opcode = 0x00;
  while (emu->cycles_needed > emu->cycles_executed) {
    opcode = emu_fetch_byte(emu);

    switch (opcode) {

      // BRK
    case OPCODE_BRK: {
      PRINT_STAT("Force Interrupt (BRK)\n");
      goto _end_emulation;
    } break;

      // LDA
    case OPCODE_LDA_IM: {
      u8 data = emu_fetch_byte(emu);
      emu->cpu.a = data;
      emu_update_flags_a(emu);
      emu->cycles_executed += 2;
    } break;

    case OPCODE_LDA_ZP: {
      u8 addr = emu_fetch_byte(emu);
      emu->cpu.a = emu_read_mem_byte(emu, addr);
      emu_update_flags_a(emu);
      emu->cycles_executed += 3;
    } break;

    case OPCODE_LDA_ZPX: {
      u8 addr = emu_fetch_byte(emu);
      addr += emu->cpu.x;
      emu->cpu.a = emu_read_mem_byte(emu, addr);
      emu_update_flags_a(emu);
      emu->cycles_executed += 4;
    } break;

    case OPCODE_LDA_ABS: {
      u16 addr = emu_fetch_word(emu);
      emu->cpu.a = emu_read_mem_byte(emu, addr);
      emu_update_flags_a(emu);
      emu->cycles_executed += 4;
    } break;

    case OPCODE_LDA_ABSX: {
      u16 addr = emu_fetch_word(emu);
      u16 new_addr = addr + emu->cpu.x;
      if ((addr & 0xFF00) != (new_addr & 0xFF00)) {
        // page changes, add one cycle
        emu->cycles_executed++;
      }
      emu->cpu.a = emu_read_mem_byte(emu, new_addr);
      emu_update_flags_a(emu);
      emu->cycles_executed += 4;
    } break;

    case OPCODE_LDA_ABSY: {
      u16 addr = emu_fetch_word(emu);
      u16 new_addr = addr + emu->cpu.y;
      if ((addr & 0xFF00) != (new_addr & 0xFF00)) {
        // page changes, add one cycle
        emu->cycles_executed++;
      }
      emu->cpu.a = emu_read_mem_byte(emu, new_addr);
      emu_update_flags_a(emu);
      emu->cycles_executed += 4;
    } break;

    case OPCODE_LDA_INDX: {
      u16 addr0 = emu_fetch_byte(emu);
      addr0 += emu->cpu.x;
      u16 addr = emu_read_mem_word(emu, addr0);
      emu->cpu.a = emu_read_mem_byte(emu, addr);
      emu_update_flags_a(emu);
      emu->cycles_executed += 6;
    } break;

    case OPCODE_LDA_INDY: {
      u16 addr0 = emu_fetch_word(emu);
      u16 addr = emu_read_mem_word(emu, addr0);
      u16 new_addr = addr + emu->cpu.y;
      if ((addr & 0xFF00) != (new_addr & 0xFF00)) {
        // page changes, add one cycle
        emu->cycles_executed++;
      }
      emu->cpu.a = emu_read_mem_byte(emu, new_addr);
      emu_update_flags_a(emu);
      emu->cycles_executed += 5;
    } break;

      // LDX
    case OPCODE_LDX_IM: {
      u8 data = emu_fetch_byte(emu);
      emu->cpu.x = data;
      emu_update_flags_x(emu);
      emu->cycles_executed += 2;
    } break;

    case OPCODE_LDX_ZP: {
      u8 addr = emu_fetch_byte(emu);
      emu->cpu.x = emu_read_mem_byte(emu, addr);
      emu_update_flags_x(emu);
      emu->cycles_executed += 3;
    } break;

    case OPCODE_LDX_ZPY: {
      u8 addr = emu_fetch_byte(emu);
      addr += emu->cpu.y;
      emu->cpu.x = emu_read_mem_byte(emu, addr);
      emu_update_flags_x(emu);
      emu->cycles_executed += 4;
    } break;

    case OPCODE_LDX_ABS: {
      u16 addr = emu_fetch_word(emu);
      emu->cpu.x = emu_read_mem_byte(emu, addr);
      emu_update_flags_x(emu);
      emu->cycles_executed += 4;
    } break;

    case OPCODE_LDX_ABSY: {
      u16 addr = emu_fetch_word(emu);
      u16 new_addr = addr + emu->cpu.y;
      if ((addr & 0xFF00) != (new_addr & 0xFF00)) {
        // page changes, add one cycle
        emu->cycles_executed++;
      }
      emu->cpu.x = emu_read_mem_byte(emu, new_addr);
      emu_update_flags_x(emu);
      emu->cycles_executed += 4;
    } break;

      // LDY
    case OPCODE_LDY_IM: {
      u8 data = emu_fetch_byte(emu);
      emu->cpu.y = data;
      emu_update_flags_y(emu);
      emu->cycles_executed += 2;
    } break;

    case OPCODE_LDY_ZP: {
      u8 addr = emu_fetch_byte(emu);
      emu->cpu.y = emu_read_mem_byte(emu, addr);
      emu_update_flags_y(emu);
      emu->cycles_executed += 3;
    } break;

    case OPCODE_LDY_ZPY: {
      u8 addr = emu_fetch_byte(emu);
      addr += emu->cpu.y;
      emu->cpu.y = emu_read_mem_byte(emu, addr);
      emu_update_flags_y(emu);
      emu->cycles_executed += 4;
    } break;

    case OPCODE_LDY_ABS: {
      u16 addr = emu_fetch_word(emu);
      emu->cpu.y = emu_read_mem_byte(emu, addr);
      emu_update_flags_y(emu);
      emu->cycles_executed += 4;
    } break;

    case OPCODE_LDY_ABSY: {
      u16 addr = emu_fetch_word(emu);
      u16 new_addr = addr + emu->cpu.y;
      if ((addr & 0xFF00) != (new_addr & 0xFF00)) {
        // page changes, add one cycle
        emu->cycles_executed++;
      }
      emu->cpu.y = emu_read_mem_byte(emu, new_addr);
      emu_update_flags_y(emu);
      emu->cycles_executed += 4;
    } break;

      // JMP
    case OPCODE_JMP_ABS: {
      PRINT_STAT("JMP instruction\n");
      u16 addr = emu_fetch_word(emu);
      emu->cpu.pc = addr;
      emu->cycles_executed += 3;
    } break;

    case OPCODE_JMP_IND: {
      PRINT_STAT("JMP instruction\n");
      u16 addr = emu_read_mem_word(emu, emu_fetch_word(emu));
      emu->cpu.pc = addr;
      emu->cycles_executed += 5;
    } break;

    default: {
      PRINT_STAT("Instruction not handled\n");
      goto _end_emulation;
    } break;
    }
  }
  PRINT_STAT("Cycles reached\n");
_end_emulation:
  return;
}

i32 main() {
  Emulator emu;
  emu_init(&emu, 0xFFFFFFFFFFFFFFFF);

  emu.mem[0x1000] = OPCODE_LDY_IM; // Load Y Immediate Addressing
  emu.mem[0x1001] = -42;
  emu.mem[0x1002] = OPCODE_LDX_IM; // Load X Immediate Addressing
  emu.mem[0x1003] = 1;
  emu.mem[0x1004] = OPCODE_LDA_ABSX; // Load A Absolute Addressing + X
  emu.mem[0x1005] = 0x20;
  emu.mem[0x1006] = 0x00;

  emu.mem[0x2001] = 0xFF;

  // Starting point
  emu.mem[0xFFFC] = OPCODE_JMP_ABS; // Jump Absolute Addressing
  emu.mem[0xFFFD] = 0x10;
  emu.mem[0xFFFE] = 0x00;

  emu_execute(&emu);
  return 0;
}
