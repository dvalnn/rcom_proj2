#ifndef __LINKLAYER_H__
#define __LINKLAYER_H__

#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "frames.h"
#include "log.h"
#include "sds.h"

typedef struct _linkLayer {
    char serialPort[50];
    int payload_size;
    int baudRate;
    int numTries;
    int timeOut;
    int role;  // defines the role of the program: 0==Transmitter, 1=Receiver
    int fd;
    struct termios oldtio;
} linkLayer;

// ROLE
#define NOT_DEFINED -1
#define TRANSMITTER 0
#define RECEIVER 1

// SIZE of maximum acceptable payload; maximum number of bytes that application layer should send to link layer
#define MAX_PAYLOAD_SIZE 1000

// CONNECTION deafault values
#define BAUDRATE_DEFAULT B9600
#define MAX_RETRANSMISSIONS_DEFAULT 3
#define TIMEOUT_DEFAULT 4
#define _POSIX_SOURCE 1 /* POSIX compliant source */

// Opens a connection using the "port" parameters defined in struct linkLayer, returns "-1" on error and "1" on sucess
int llopen(linkLayer* connectionParameters);
// Sends data in buf with size bufSize
int llwrite(linkLayer ll, char* filepath);
// Receive data in packet
int llread(linkLayer ll, char* filename);
// Closes previously opened connection; if showStatistics==TRUE, link layer should print statistics in the console on close
int llclose(linkLayer connectionParameters, int showStatistics);

#endif
