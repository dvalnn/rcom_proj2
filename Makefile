# -*- Makefile -*-
# Makefile to build the project

# Parameters
CC = gcc

DEBUG_LEVEL = 3

# _DEBUG is used to include internal logging of errors and general information. Levels go from 1 to 3, highest to lowest priority respectively
# _PRINT_PACKET_DATA is used to print the packet data that is received by RX
CFLAGS = -Wall -g -D _DEBUG=$(DEBUG_LEVEL)

SRC = src
LIB = lib
INCLUDE = include
BIN = bin

RECEIVER = noncanonical.c
TRANSMITTER = writenoncanonical.c
BUILDEXTENS = out

SERIAL1 = /dev/ttyS10
SERIAL2 = /dev/ttyS11

# Targets
.PHONY: all

all: $(BIN)/receiver.$(BUILDEXTENS) $(BIN)/transmitter.$(BUILDEXTENS)

$(BIN)/receiver.$(BUILDEXTENS): $(SRC)/$(RECEIVER) #$(LIB)/*.c
	$(CC) $(CFLAGS) -o $@ $^ -I$(INCLUDE) -lrt

$(BIN)/transmitter.$(BUILDEXTENS): $(SRC)/$(TRANSMITTER) #$(LIB)/*.c
	$(CC) $(CFLAGS) -o $@ $^ -I$(INCLUDE) -lrt

.PHONY: clean
clean:
	rm -f $(BIN)/*

.PHONY: socat
socat: 
	sudo socat -d -d PTY,link=/dev/ttyS10,mode=777 PTY,link=/dev/ttyS11,mode=777

.PHONY: runt
runt:
	./$(BIN)/transmitter.$(BUILDEXTENS) $(SERIAL1)

.PHONY: runr
runr:
	./$(BIN)/receiver.$(BUILDEXTENS) $(SERIAL2)

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