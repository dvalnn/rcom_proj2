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
            if (rcved == F)
                new_state = st_FLAG_RCV;
            break;

        case st_FLAG_RCV:
            if (rcved == A)
                new_state = st_A_RCV;
            else if (rcved != F)
                new_state = st_START;
            break;

        case st_A_RCV:
            if (rcved == C)
                new_state = st_C_RCV;
            if (rcved == F)
                new_state = st_FLAG_RCV;
            else
                new_state = st_START;
            break;

        case st_C_RCV:
            if (rcved == BCC)
                new_state = st_BCC_OK;
            if (rcved == F)
                new_state = st_FLAG_RCV;
            else
                new_state = st_START;
            break;

        case st_BCC_OK:
            if (rcved == F)
                new_state = st_STOP;
            else
                new_state == st_START;
            break;

        case st_STOP:
            break;
    }

    return new_state;
}

void state_handler(int fd) {
    states cur_state = st_START;

    unsigned char flag;

    while (cur_state != st_STOP) {
        LOG("Current State: %d\n", cur_state);
        short byte = read(fd, flag, 1);
        if (byte != 1)
            ERROR("Could not read char from file descriptor\n");
        cur_state = state_update(cur_state, flag);
    }
    write(fd, "WAH", sizeof("WAH"));
}

#undef F
#undef A
#undef C
#undef BCC

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
