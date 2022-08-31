#include "common.h"
#include "opcode.h"

// 64 kB
#define MEM_SIZE 65536

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

typedef struct Emulator {
  CPU cpu;
  u8 mem[MEM_SIZE];
  u64 cycles;
  bool is_running;
} Emulator;

// Initialize the memory
// Memory size must be `MEM_SIZE`
void mem_init(u8 *mem);

// Resets the CPU to its initial state
void cpu_reset(CPU *cpu);

// Get the status flags of the CPU as a single byte
u8 cpu_stat_get_byte(CPU *cpu);

// Set the status flags from the output of `cpu_stat_get_byte(...)`
void cpu_set_stat_from_byte(CPU *cpu, u8 stat);

// Outputs the registers and status flags of a CPU
// Output has newline characters
void cpu_debug_print(CPU *cpu);

// Initialize the emulator
void emu_init(Emulator *emu);

// Fetch 1 byte from memory on position of PC
u8 emu_fetch_byte(Emulator *emu);
// Fetch 2 bytes from memory on position of PC
u16 emu_fetch_word(Emulator *emu);

// Update CPU flags based on the Accumulator
void emu_update_flags_a(Emulator *emu);
// Update CPU flags based on the X register
void emu_update_flags_x(Emulator *emu);
// Update CPU flags based on the Y register
void emu_update_flags_y(Emulator *emu);

// Outputs the stack (memory address 0x0100 ~ 0x01FF)
// Output has newline characters
void emu_print_stack(Emulator *emu);

// Read a byte of data from memory on address `addr`
u8 emu_read_mem_byte(Emulator *emu, u16 addr);
// Read 2 bytes of data from memory on address `addr`
u16 emu_read_mem_word(Emulator *emu, u16 addr);

// Execute one instruction
// `debug_output`: whether or not to output debug information for every interrupt and jump instruction
void emu_tick(Emulator *emu, bool debug_output);
