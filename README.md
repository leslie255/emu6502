# Leslie255's Commodore 6502 Emulator

Progress of this project is recorded in `progress.txt`.

A reference sheet for the MOS 6502 instruction set can be found at: [https://www.masswerk.at/6502/6502_instruction_set.html](https://www.masswerk.at/6502/6502_instruction_set.html)

## How to Use

There is a `MemWriter` struct and it's associated functions in `main.c` to write things into the emulator memory. Write the program into the memory, and then build and run the project by:

```bash
$ make all && ./bin/emu6502
```

An assembler is also going to be added

## LICENSE

This project is licensed under GPLv3, earlier versions that did not have the `LICENSE.txt` is licensed under ALL RIGHTS RESERVED.
