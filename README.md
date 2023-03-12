# Leslie255's Commodore 6502 Emulator

Progress of this project is recorded in `progress.txt`.

A reference sheet for the MOS 6502 instruction set can be found at: [https://www.masswerk.at/6502/6502_instruction_set.html](https://www.masswerk.at/6502/6502_instruction_set.html)

## How to Use

There is a `MemWriter` struct and it's associated functions in `main.c` to write things into the emulator memory. Write the program into the memory, and then build and run the project by:

```bash
$ make all && ./bin/emu6502
```

The emulator outputs a debug interface which traces the CPU and stack step by step, which can slow down the performance significantly. To disable that, add `--less-io` option to only output a `{x} cycles` prompt every 8 million ticks.

An assembler is also going to be added.

## LICENSE

This project is licensed under GPLv3, earlier versions that did not have the `LICENSE.txt` is licensed under ALL RIGHTS RESERVED.
