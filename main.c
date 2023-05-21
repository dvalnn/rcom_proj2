#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log.h"

#include "linklayer.h"

/*
 * $1 /dev/ttySxx
 * $2 tx | rx
 * $3 filename
 */

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("usage: progname /dev/ttySxx tx|rx filename\n");
        exit(1);
    }

    linkLayer ll;
    ll.baudRate = BAUDRATE_DEFAULT;
    ll.numTries = MAX_RETRANSMISSIONS_DEFAULT;
    ll.timeOut = TIMEOUT_DEFAULT;
    ll.payload_size = MAX_PAYLOAD_SIZE;
    ll.role = NOT_DEFINED;
    strcpy(ll.serialPort, argv[1]);

    if (strcmp(argv[2], "tx") == 0)
        ll.role = TRANSMITTER;
    else if (strcmp(argv[2], "rx") == 0)
        ll.role = RECEIVER;
    else {
        printf("usage: progname /dev/ttySxx tx|rx filename\n");
        exit(1);
    }

    if (llopen(&ll) < 0) {
        ERROR("Error opening connection\n");
        exit(1);
    }

    switch (ll.role) {
        case TRANSMITTER:
            if (llwrite(ll, argv[3]) < 0) {
                ERROR("Error sending file\n");
                exit(1);
            }
            break;
        case RECEIVER:
            while (true) {
                int retval = llread(ll, argv[3]);
                if (retval < 0) {
                    ERROR("Error receiving file\n");
                    break;
                } else if (retval == 0)
                    break;
            }
            break;
    }

    if (llclose(ll, true) < 0) {
        ERROR("Error closing connection\n");
        exit(1);
    }
}