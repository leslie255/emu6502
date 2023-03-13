CC = gcc
CFLAGS = -Wall --std=gnu2x -lncurses  -O2

all: bin/main.o bin/emu6502.o bin/emu6502

bin/main.o: src/main.c src/common.h src/opcode.h
	$(CC) $(CFLAGS) -c src/main.c -o bin/main.o

bin/emu6502.o: src/emu6502.c src/emu6502.h src/common.h src/opcode.h
	$(CC) $(CFLAGS) -c src/emu6502.c -o bin/emu6502.o

bin/emu6502: bin/main.o bin/emu6502.o
	$(CC) $(CFLAGS) bin/*.o -o bin/emu6502
