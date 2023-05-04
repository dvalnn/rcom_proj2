/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>

#include "log.h"
#include "suFrames.h"

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */

#define BUF_SIZE 255

int serial_open(char* serial_port);
void serial_config(int fd, struct termios* oldtio);
void serial_close(int fd, struct termios* configs);

#pragma region SET_Con

typedef enum states {
    st_START,
    st_FLAG_RCV,
    st_A_RCV,
    st_C_RCV,
    st_BCC1_OK,
    st_INFO_RCV,
    st_BCC2_OK,
    st_STOP
} states;

states state_update(states cur_state, unsigned char rcved) {
    states new_state = cur_state;
    uChar set_msg[] = SET(A1);

    switch (cur_state) {
        case st_START:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", set_msg[0], (unsigned int)(rcved & 0xFF));
            if (rcved == set_msg[0])
                new_state = st_FLAG_RCV;
            break;

        case st_FLAG_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", set_msg[1], (unsigned int)(rcved & 0xFF));
            if (rcved == set_msg[1])
                new_state = st_A_RCV;
            else if (rcved != set_msg[0])
                new_state = st_START;
            break;

        case st_A_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", set_msg[2], (unsigned int)(rcved & 0xFF));
            if (rcved == set_msg[2])
                new_state = st_C_RCV;
            else if (rcved == set_msg[0])
                new_state = st_FLAG_RCV;
            else
                new_state = st_START;
            break;

        case st_C_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", set_msg[3], (unsigned int)(rcved & 0xFF));
            if (rcved == set_msg[3])
                new_state = st_BCC1_OK;
            else if (rcved == set_msg[0])
                new_state = st_FLAG_RCV;
            else
                new_state = st_START;
            break;

        case st_BCC1_OK:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", set_msg[4], (unsigned int)(rcved & 0xFF));
            if (rcved == set_msg[4])
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
    if (argc < 2) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    (void)signal(SIGALRM, on_alarm);

    struct termios oldtio;
    int fd = serial_open(argv[1]);
    serial_config(fd, &oldtio);

    INFO("New termios structure set.\n");

    LOG("Awaiting SET/UA connection.\n");

    while (true) {
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

    serial_close(fd, &oldtio);

    return 0;
}

int serial_open(char* serial_port) {
    int fd;

    (void)signal(SIGALRM, on_alarm);

    fd = open(serial_port, O_RDWR | O_NOCTTY);

    if (fd < 0) {
        ERROR("%s", serial_port);
        exit(-1);
    }

    return fd;
}

void serial_config(int fd, struct termios* oldtio) {
    struct termios newtio;

    LOG("SAVING terminal settings\n");

    sleep(1);
    if (tcgetattr(fd, oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    LOG("Settings saved\n");

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    //* VTIME - Timeout in deciseconds for noncanonical read.
    //* VMIN - Minimum number of characters for noncanonical read.
    // VTIME = 1 para esperar 100 ms por read
    newtio.c_cc[VTIME] = 1;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        ERROR("tcsetattr");
        exit(-1);
    }

    LOG("New termios structure set.\n");
}

void serial_close(int fd, struct termios* configs) {
    if (tcsetattr(fd, TCSANOW, configs) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
}