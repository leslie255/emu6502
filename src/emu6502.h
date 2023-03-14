#include "common.h"
#include "opcode.h"

// 64 kB
#define MEM_SIZE 65536

#define STACK_FLOOR 0x0100
#define STACK_LIMIT 0x01FF

typedef struct CPU {
  u16 pc;      // Program Counter
  u8 sp;       // Stack Pointer
  u8 a;        // Register A
  u8 x;        // Register X
  u8 y;        // Register Y
  bool flag_c; // Carry Flag
  bool flag_z; // Zero Flag
  bool flag_i; // Interrupt Disable
  bool flag_d; // Decimal Mode
  bool flag_b; // Break Command
  bool flag_v; // Overflow Flag
  bool flag_n; // Negative Flag
} CPU;

typedef struct Emulator {
  CPU cpu;
  u8 mem[MEM_SIZE];
  u64 cycles;
  bool is_running;
} Emulator;

// Initialize the memory
// Memory size must be `MEM_SIZE`
void mem_init(u8 *mem);

// Resets the CPU flags to all zeros
void cpu_reset_sr(CPU *cpu);

// Resets the CPU to its initial state
void cpu_reset(CPU *cpu);

// Get the status flags of the CPU as a single byte
u8 cpu_get_sr(const CPU *cpu);

// Set the status flags from a single byte, in the same format of
// `cpu_stat_get_byte(...)`
void cpu_set_sr_from_byte(CPU *cpu, const u8 stat);

// Outputs the registers and status flags of a CPU
// Output has newline characters
void cpu_debug_print(const CPU *cpu);

// Initialize the emulator
void emu_init(Emulator *emu);

// Outputs the stack (memory address 0x0100 ~ 0x01FF)
// Output has newline characters
void emu_print_stack(const Emulator *emu);

// Read a byte of data from memory on address `addr`
u8 emu_read_mem_byte(const Emulator *emu, u16 addr);
// Read 2 bytes of data from memory on address `addr`
u16 emu_read_mem_word(const Emulator *emu, u16 addr);

// Execute one instruction
// `debug_output`: whether or not to output debug information for every
// interrupt and jump instruction
void emu_tick(Emulator *emu, bool debug_output);
