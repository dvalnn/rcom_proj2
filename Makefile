# -*- Makefile -*-
# Makefile to build the project

# Parameters
CC = gcc

DEBUG_LEVEL = 1

# _DEBUG is used to include internal logging of errors and general information. Levels go from 1 to 3, highest to lowest priority respectively
CFLAGS = -Wall -Wno-unknown-pragmas -Wno-implicit-function-declaration -Wno-unused-variable -g -D _DEBUG=$(DEBUG_LEVEL)

SRC = src
INCLUDE = include
BIN = bin

APP = main.c
BUILDEXTENS = exe

SERIAL1 = /dev/ttyS10
SERIAL2 = /dev/ttyS11

# INFILE = files/wywh.txt
# OUTFILE = received.txt

# INFILE = files/penguin.gif
# OUTFILE = received.gif

INFILE = files/goat.jpg
OUTFILE = received.jpg

# Targets
.PHONY: all
all: $(BIN)/app.$(BUILDEXTENS)

$(BIN)/app.$(BUILDEXTENS): $(APP) $(SRC)/*.c
	$(CC) $(CFLAGS) -o $@ $^ -I$(INCLUDE) -lrt

.PHONY: socat
socat: 
	sudo socat -d -d PTY,link=/dev/ttyS10,mode=777 PTY,link=/dev/ttyS11,mode=777

.PHONY: tx
tx:
	./$(BIN)/app.$(BUILDEXTENS) $(SERIAL1) tx $(INFILE)

.PHONY: rx
rx:
	./$(BIN)/app.$(BUILDEXTENS) $(SERIAL2) rx $(OUTFILE)

.PHONY: clean
clean:
	rm -f $(BIN)/*
	rm $(OUTFILE)
