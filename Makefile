# -*- Makefile -*-
# Makefile to build the project

# Parameters
CC = gcc

DEBUG_LEVEL=2

# _DEBUG is used to include internal logging of errors and general information. Levels go from 1 to 3, highest to lowest priority respectively
# _PRINT_PACKET_DATA is used to print the packet data that is received by RX
CFLAGS = -Wall -g -D _DEBUG=$(DEBUG_LEVEL)

SRC = src
INCLUDE = include
BIN = bin

# Targets

.PHONY: all

all: $(BIN)/receiver.out $(BIN)/transmitter.out

$(BIN)/receiver.out: $(SRC)/noncanonical.c
	$(CC) $(CFLAGS) -o $@ $^ -I$(INCLUDE) -lrt

$(BIN)/transmitter.out: $(SRC)/writenoncanonical.c
	$(CC) $(CFLAGS) -o $@ $^ -I$(INCLUDE) -lrt

#.PHONY: all
#all: $(BIN)/main
#
#$(BIN)/main: main.c $(SRC)/*.c
#	$(CC) $(CFLAGS) -o $@ $^ -I$(INCLUDE) -lrt
#
#.PHONY: run
#run: $(BIN)/main
#	./$(BIN)/main
#
#docs: $(BIN)/main
#	doxygen Doxyfile
#

.PHONY: clean
clean:
	rm -f $(BIN)/*

.Phony: socat
socat: 
	sudo socat -d -d PTY,link=/dev/ttyS10,mode=777 PTY,link=/dev/ttyS11,mode=777