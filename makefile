CC = gcc
CFLAGS = -Wall --std=gnu2x

all: bin/main.o bin/emu6502

bin/main.o: src/main.c src/common.h src/opcode.h
	$(CC) $(CFLAGS) -c src/main.c -o bin/main.o

bin/emu6502: bin/main.o
	$(CC) $(CFLAGS) bin/*.o -o bin/emu6502
