#include "common.h"
#include "opcode.h"

// 64 kB
#define MEM_SIZE 65536

void mem_init(u8 *mem);

typedef struct CPU {
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

void cpu_reset(CPU *cpu);

u8 cpu_stat_get_byte(CPU *cpu);

void cpu_set_stat_from_byte(CPU *cpu, u8 stat);

void cpu_debug_print(CPU *cpu);

typedef struct Emulator {
  CPU cpu;
  u8 mem[MEM_SIZE];
  u64 cycles;
  bool is_running;
} Emulator;

void emu_init(Emulator *emu);

// fetch 1 byte from memory on position of PC
u8 emu_fetch_byte(Emulator *emu);
// fetch 2 bytes from memory on position of PC
u16 emu_fetch_word(Emulator *emu);

void emu_update_flags_a(Emulator *emu);
void emu_update_flags_x(Emulator *emu);
void emu_update_flags_y(Emulator *emu);

void emu_print_stack(Emulator *emu);

// read a byte of data from memory on address `addr`
u8 emu_read_mem_byte(Emulator *emu, u16 addr);
// read 2 bytes of data from memory on address `addr`
u16 emu_read_mem_word(Emulator *emu, u16 addr);

// execute one instruction
void emu_tick(Emulator *emu, bool debug_output);
