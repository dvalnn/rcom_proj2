#ifndef _SERIAL_H_
#define _SERIAL_H_

#include "log.h"

#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>

#define BAUDRATE B38400

int serial_open(char* serial_port);
void serial_config(int fd, struct termios* oldtio);
void serial_close(int fd, struct termios* configs);

#endif