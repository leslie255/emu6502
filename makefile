CC = gcc
CFLAGS = -Wno-unused-command-line-argument -Wall -Wconversion --std=gnu2x -lncurses

OPT_LEVEL = -O2

all: bin/main.o bin/emu6502.o bin/emu6502

bin/main.o: src/main.c src/common.h src/opcode.h src/calc.h
	$(CC) $(CFLAGS) $(OPT_LEVEL) -c src/main.c -o bin/main.o

bin/emu6502.o: src/emu6502.c src/emu6502.h src/common.h src/opcode.h src/calc.h
	$(CC) $(CFLAGS) $(OPT_LEVEL) -c src/emu6502.c -o bin/emu6502.o

bin/emu6502: bin/main.o bin/emu6502.o
	$(CC) $(CFLAGS) $(OPT_LEVEL) bin/*.o -o bin/emu6502
