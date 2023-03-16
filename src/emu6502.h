#include "common.h"
#include "opcode.h"

// 64 kB
#define MEM_SIZE 65536

#define STACK_FLOOR 0x0100
#define STACK_LIMIT 0x01FF

typedef struct CPU {
  u16 pc; // Program Counter
  u8 sp;  // Stack Pointer
  u8 a;   // Register A
  u8 x;   // Register X
  u8 y;   // Register Y
  union {
    u8 byte;
    struct {
      bool n : 1;
      bool v : 1;
      bool _ : 1;
      bool b : 1;
      bool d : 1;
      bool i : 1;
      bool z : 1;
      bool c : 1;
    } bits;
  } sr;
} CPU;

#define LOG_BUF_SIZE 1024

typedef struct Emulator {
  CPU cpu;
  u8 mem[MEM_SIZE];
  u64 cycles;
  bool is_running;
  bool debug_output;
  char log_buf[LOG_BUF_SIZE];
} Emulator;

// Initialize the memory
// Memory size must be `MEM_SIZE`
void mem_init(u8 *mem);

// Resets the CPU flags to all zeros
void cpu_reset_sr(CPU *cpu);

// Resets the CPU to its initial state
void cpu_reset(CPU *cpu);

// Outputs the registers and status flags of a CPU
// Output has newline characters
void cpu_debug_print(const CPU *cpu);

// Initialize the emulator
void emu_init(Emulator *emu, bool debug_output);

// Outputs the stack (memory address 0x0100 ~ 0x01FF)
// Output has newline characters
void emu_print_stack(const Emulator *emu);

// Read a byte of data from memory on address `addr`
u8 emu_read_mem_byte(const Emulator *emu, u16 addr);
// Read 2 bytes of data from memory on address `addr`
u16 emu_read_mem_word(const Emulator *emu, u16 addr);

// Execute one instruction
void emu_tick(Emulator *emu);
