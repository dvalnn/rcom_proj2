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
#define MAX_RETRIES 5

bool alarm_flag = false;

void serial_config(int fd, struct termios* oldtio);

void serial_close(int fd, struct termios* configs);

int serial_open(char* serial_port);

void on_alarm();  // atende alarme

#pragma region SET_Con

#define F 0x5C
#define A 0x01
#define C 0x03
#define BCC (A ^ C)

void set_connection(int fd) {
    unsigned char set[] = {F, A, C, BCC, F};
    write(fd, set, sizeof(set));
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

typedef enum ua_states {
    ua_START,
    ua_FLAG_RCV,
    ua_A_RCV,
    ua_C_RCV,
    ua_BCC_OK,
    ua_STOP
} ua_state;

ua_state ua_state_update(ua_state cur_state, unsigned char rcved) {
    ua_state new_state = cur_state;

    switch (cur_state) {
        case ua_START:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", F, (unsigned int)(rcved & 0xFF));
            if (rcved == F)
                new_state = ua_FLAG_RCV;
            break;

        case ua_FLAG_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", A, (unsigned int)(rcved & 0xFF));
            if (rcved == A)
                new_state = ua_A_RCV;
            else if (rcved != F)
                new_state = ua_START;
            break;

        case ua_A_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", C, (unsigned int)(rcved & 0xFF));
            if (rcved == C)
                new_state = ua_C_RCV;
            else if (rcved == F)
                new_state = ua_FLAG_RCV;
            else
                new_state = ua_START;
            break;

        case ua_C_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", BCC, (unsigned int)(rcved & 0xFF));
            if (rcved == BCC)
                new_state = ua_BCC_OK;
            else if (rcved == F)
                new_state = ua_FLAG_RCV;
            else
                new_state = ua_START;
            break;

        case ua_BCC_OK:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", F, (unsigned int)(rcved & 0xFF));
            if (rcved == F)
                new_state = ua_STOP;
            else
                new_state = ua_START;
            break;

        case ua_STOP:
            break;
    }

    return new_state;
}

int parse_message(unsigned char* message, int message_length) {
    ua_state cur_state = ua_START;
    printf("\n");
    INFO("--UA Detection START--\n");

    for (int i = 0; i < message_length; i++) {
        LOG("\tChar received: 0x%.02x, same as: %c (position %d)\n", message[i], (unsigned int)(message[i] & 0xff), i);
        cur_state = ua_state_update(cur_state, message[i]);
        LOG("\tCurrent state: %d\n", cur_state);
        if (cur_state == ua_STOP)
            break;
    }

    return cur_state == ua_STOP;
}

int ua_connection(int fd) {
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

bool establish_connection(int fd) {
    for (int tries = 1; tries < MAX_RETRIES; tries++) {
        LOG("Attemping SET/UA connection.\n\t- Attempt number: %d\n", tries);
        set_connection(fd);

        alarm(3);

        while (!alarm_flag) {
            if (ua_connection(fd)) {
                alarm(0);
                return true;
            }
        }

        if (!alarm_flag)
            break;

        ALARM("SET/UA Connection failed.\n\t- Trying again in 2 seconds.\n");
        alarm_flag = false;
        sleep(2);
    }

    return false;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        INFO("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    (void)signal(SIGALRM, on_alarm);

    struct termios oldtio;

    int fd = serial_open(argv[1]);
    serial_config(fd, &oldtio);

    if (!establish_connection(fd)) {
        INFO("Failed to establish serial port connection.\n");
        exit(-1);
    }

    INFO("SET/UA successfull.\n");
    INFO("Waiting for user input. [max 255 chars]\n");

    // int bytes_read = 0;
    // char buf[BUF_SIZE];
    // while (scanf(" %254[^\n]%n", buf, &bytes_read) == 1) {
    //     if (bytes_read > 0) {
    //         int bytes_written = write(fd, buf, bytes_read);
    //         LOG("\t>%d bytes sent\n", bytes_written);

    //         if (buf[0] == 'z' && bytes_written <= 2) {
    //             break;
    //         }
    //         bytes_read = read(fd, buf, bytes_written);  // read receiver echo message
    //         LOG("\t>Echo received (%d chars): %s\n", bytes_read, buf);
    //     }
    // }

    serial_close(fd, &oldtio);

    return 0;
}

void on_alarm()  // atende alarme
{
    ALARM("Alarm Interrupt Triggered\n");
    alarm_flag = true;
}

void serial_config(int fd, struct termios* oldtio) {
    struct termios newtio;

    LOG("SAVING terminal settings");

    sleep(1);
    if (tcgetattr(fd, oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    LOG("Settings saved");

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

void serial_close(int fd, struct termios* configs) {
    if (tcsetattr(fd, TCSANOW, configs) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
}