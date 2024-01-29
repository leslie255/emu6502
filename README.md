# Leslie255's Commodore 6502 Emulator

A reference sheet for the MOS 6502 instruction set can be found at: [https://www.masswerk.at/6502/6502_instruction_set.html](https://www.masswerk.at/6502/6502_instruction_set.html)

## How to Use

There is a `MemWriter` struct and it's associated functions in `main.c` to write things into the emulator memory. Write the program into the memory, and then build and run the project by:

```bash
$ mkdir bin
$ make all && ./bin/emu6502
```

There is also a `--dbg` option for instruction-by-instruction running while printing the entire stack, values of registers, etc.

Note that the emulator likely won't work in big endian platforms.

## Future Plans

All the instructions have been implemented by now, but there are still some extra work to do to make the emulator actually useful, namely:

- IO & Interrupts *(Currently `BRK` and `RTI` instructions technically work, but the emulator cannot be recovered from an interrupt)*
- Clockspeed limiter
- Loading from memory/disk snapshots
- An assembler

## LICENSE

This project is licensed under GPLv3, earlier versions that did not have the `LICENSE.txt` is licensed under ALL RIGHTS RESERVED.
