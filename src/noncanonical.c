/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "log.h"

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define BUF_SIZE 255

volatile int STOP = FALSE;

// Global Variables for alarms
// int alarm_flag = 1, alarm_counter = 1;

#pragma region SET_Con

#define F 0x5C
#define A 0x01
#define C 0x03
#define BCC (A ^ C)

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
    INFO("--Set Detection START--\n");

    for (int i = 0; i < message_length; i++) {
        LOG("\tChar received: 0x%.02x (position %d)\n", (unsigned int)(message[i] & 0xff), i);
        cur_state = state_update(cur_state, message[i]);
        LOG("\tCurrent state: %d\n", cur_state);
        if (cur_state == st_STOP)
            break;
    }

    return cur_state == st_STOP;
}

int read_incomming(int fd) {
    unsigned char buf[BUF_SIZE];
    int lenght = read(fd, buf, sizeof(buf));
    if (lenght) {
        LOG("Received %d characters\n", lenght);
        return parse_message(buf, lenght);
    }
    return 0;
}
#undef F
#undef A
#undef C
#undef BCC

#pragma endregion

#pragma region UA_Con

#define F 0x5C
#define A 0x01
#define C 0x07
#define BCC (A ^ C)

void ua_message(int fd) {
    unsigned char set[] = {F, A, C, BCC, F};
    write(fd, set, sizeof(set));
}

#undef F
#undef A
#undef C
#undef BCC

#pragma endregion

#pragma region Alarm Control

void on_alarm()  // atende alarme
{
    ALARM("Alarm Interrupt Triggered\n");
    // alarm_flag = 1;
    // alarm_counter++;
}

#pragma endregion

int main(int argc, char** argv) {
    // int fd, c, res;
    int fd;  //, res;
    struct termios oldtio, newtio;
    // char buf[255];

    (void)signal(SIGALRM, on_alarm);

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

    // VTIME = 1 para esperar 100 ms por read
    //* VTIME - Timeout in deciseconds for noncanonical read.
    //* VMIN - Minimum number of characters for noncanonical read.
    newtio.c_cc[VTIME] = 1;
    newtio.c_cc[VMIN] = 0;

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
    LOG("Awaiting SET/UA connection.");

    while (1) {
        if (read_incomming(fd)) {
            LOG("SET successfull\n");
            ua_message(fd);
            break;
        }
    }

    // while (STOP == FALSE) {       /* loop for input */
    //     res = read(fd, buf, 255); /* returns after 255 chars have been input */
    //     buf[res] = '\0';          /* so we can printf... */
    //     INFO("%s\n\t>%d chars received\n", buf, res);
    //     if (buf[0] == 'z' && res <= 2)
    //         STOP = TRUE;
    //     LOG("\techoing back... ");
    //     res = write(fd, buf, res);  // echoes back received message
    //     LOG("%d chars sent\n", res);
    // }

    /*
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no guião
    */

    tcsetattr(fd, TCSANOW, &oldtio);
    close(fd);
    return 0;
}

#undef BUF_SIZE