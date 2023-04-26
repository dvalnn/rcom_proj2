/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "macros.h"

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;

#pragma region states
typedef enum states {
    st_START,
    st_FLAG_RCV,
    st_A_RCV,
    st_C_RCV,
    st_BCC_OK,
    st_STOP
} states;

states state_update(states cur_state, unsigned char rcved) {
    states new_state = cur_state;

    switch (cur_state) {
        case st_START:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", F, (unsigned int)(rcved & 0xFF));
            if (rcved == F)
                new_state = st_FLAG_RCV;
            break;

        case st_FLAG_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", A, (unsigned int)(rcved & 0xFF));
            if (rcved == A)
                new_state = st_A_RCV;
            else if (rcved != F)
                new_state = st_START;
            break;

        case st_A_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", C, (unsigned int)(rcved & 0xFF));
            if (rcved == C)
                new_state = st_C_RCV;
            else if (rcved == F)
                new_state = st_FLAG_RCV;
            else
                new_state = st_START;
            break;

        case st_C_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", BCC, (unsigned int)(rcved & 0xFF));
            if (rcved == BCC)
                new_state = st_BCC_OK;
            else if (rcved == F)
                new_state = st_FLAG_RCV;
            else
                new_state = st_START;
            break;

        case st_BCC_OK:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", F, (unsigned int)(rcved & 0xFF));
            if (rcved == F)
                new_state = st_STOP;
            else
                new_state = st_START;
            break;

        case st_STOP:
            break;
    }

    return new_state;
}

int parse_message(unsigned char* message, int message_length) {
    states cur_state = st_START;
    printf("\n");
    LOG("--Set Detection START--\n");

    for (int i = 0; i < message_length; i++) {
        LOG("\tChar received: 0x%.02x (position %d)\n", (unsigned int)(message[i] & 0xff), i);
        cur_state = state_update(cur_state, message[i]);
        LOG("\tCurrent state: %d\n", cur_state);
        if (cur_state == st_STOP)
            break;
    }

    return cur_state == st_STOP;
}

void state_handler(int fd) {
    unsigned char buf[BUF_SIZE];
    while (1) {
        int lenght = read(fd, buf, sizeof(buf));
        LOG("Received %d characters\n", lenght);
        if (parse_message(buf, lenght))
            break;
        else
            write(fd, "Retry SET", sizeof("Retry SET"));
    }
    write(fd, "UA", sizeof("UA"));
}

// void state_handler(int fd) {
//     states cur_state = st_START;
//     states old_state = st_START;

//     unsigned char flag[] = "0";

//     while (cur_state != st_STOP) {
//         LOG("Current State: %d\n", cur_state);
//         read(fd, flag, sizeof(flag));
//         LOG("Char received: 0x%.02x\n", (unsigned int)(flag[0] & 0xff));
//         cur_state = state_update(cur_state, flag[0]);
//         if (cur_state < old_state)
//             write(fd, "Re-send", sizeof("Re-send"));
//         old_state = cur_state;
//     }
//     write(fd, "WAH", sizeof("WAH"));
// }

#pragma endregion

int main(int argc, char** argv) {
    // int fd, c, res;
    int fd, res;
    struct termios oldtio, newtio;
    char buf[255];

    if (argc < 2) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
    */

    fd = open(argv[1], O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(argv[1]);
        exit(-1);
    }

    sleep(1);
    if (tcgetattr(fd, &oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
    newtio.c_cc[VMIN] = 1;  /* blocking read until 5 chars received */

    /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) próximo(s) caracter(es)
    */

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set.\n");
    printf("Waiting on SET transmission.\n");

    state_handler(fd);

    LOG("SET successfull\n");

    while (STOP == FALSE) {       /* loop for input */
        res = read(fd, buf, 255); /* returns after 255 chars have been input */
        buf[res] = '\0';          /* so we can printf... */
        printf("%s\n\t>%d chars received\n", buf, res);
        if (buf[0] == 'z' && res <= 2)
            STOP = TRUE;
        printf("\techoing back... ");
        res = write(fd, buf, res);  // echoes back received message
        printf("%d chars sent\n", res);
    }

    /*
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no guião
    */

    tcsetattr(fd, TCSANOW, &oldtio);
    close(fd);
    return 0;
}
