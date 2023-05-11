/*Non-Canonical Input Processing*/
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>

#include "frames.h"
#include "log.h"
#include "serial.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

#define MAX_RETRIES_DEFAULT 3
#define ALARM_TIMEOUT_SEC 3
#define RETRY_INTERVAL_SEC 1

bool alarm_flag = false;

#define TIME_OUT(FUNC, RET_VAL)        \
    {                                  \
        alarm_flag = false;            \
        alarm(ALARM_TIMEOUT_SEC);      \
        while (!alarm_flag) {          \
            RET_VAL = FUNC;            \
            if (RET_VAL) {             \
                alarm(0);              \
                break;                 \
            }                          \
        }                              \
                                       \
        ALERT("Command timed out.\n"); \
    }

void alarm_handler(int signum)  // atende alarme
{
    ALERT("Alarm Interrupt Triggered with code %d\n", signum);
    alarm_flag = true;
}

typedef enum p_phases {
    establishment,
    data_transfer,
    termination
} p_phases;

void write_from_kb(int fd) {
    INFO("Waiting for user input. [max 255 chars]\n\t> ");

    int bytes_read = 0;
    char buf[255];
    while (scanf(" %254[^\n]%n", buf, &bytes_read) == 1) {
        if (bytes_read > 0) {
            int bytes_written = write(fd, buf, bytes_read);
            LOG("\t>%d bytes sent\n", bytes_written);

            if (buf[0] == 'z' && bytes_written <= 2) {
                break;
            }
        }
    }
}

void write_info(int fd, uchar* buf, int size, int id) {
    uchar msg_buf[255];
    uchar header[] = INFO_MSG(A1, id);

    for (int i = 0; i < sizeof header; i++)
        msg_buf[i] = header[i];
    for (int i = sizeof header; i < size; i++)
        msg_buf[i] = buf[i];

    msg_buf[size - 3] = F;
    msg_buf[size - 2] = F;
    msg_buf[size - 1] = '\0';

    write(fd, msg_buf, sizeof msg_buf);
}

void send_info(int fd) {
    int file = open("wywh.txt", O_RDONLY);  //! :Blushed:
    if (!file) {
        ERROR("Failed to open file 'wywh.txt'\n");
        return;
    }

    uchar buf[245];
    int id = 0;

    while (true) {  //? Tou thonking == cooking == Mr. Walter no way
        bool success = false;
        uchar response[] = RR(A1, !id);

        int size = read(file, buf, sizeof buf);
        if (!size)
            break;

        for (int i = 0; i < MAX_RETRIES_DEFAULT; i++) {
            write_info(fd, buf, size, id);
            TIME_OUT(read_incomming(fd, response), success);
            if (success)
                break;
            if (i < MAX_RETRIES_DEFAULT - 1) {
                LOG("Retrying in %d seconds\n", RETRY_INTERVAL_SEC);
                sleep(RETRY_INTERVAL_SEC);
            }
        }

        if (!success)
            break;

        id = !id;
    }

    close(file);
    return;
}

void p_phase_handler(int fd) {
    p_phases current = establishment;

    while (true) {
        switch (current) {
            case establishment: {
                uchar command[] = SET(A1);
                uchar response[] = UA(A1);
                int success = false;

                for (int i = 0; i < MAX_RETRIES_DEFAULT; i++) {
                    write(fd, command, sizeof command);
                    TIME_OUT(read_incomming(fd, response), success);
                    if (success)
                        break;
                }

                if (!success) {
                    ERROR("Failed to establish serial port connection.\n");
                    return;
                }

                INFO("Serial port connection established.\n");
                current = data_transfer;
                break;
            }

            // TODO: implementar bit stuffing
            case data_transfer: {
                send_info(fd);
                current = termination;
            }

            case termination: {
                uchar command[] = DISC(A1);
                uchar response[] = DISC(A3);
                uchar acknowledge[] = UA(A3);
                bool success = false;

                write(fd, command, sizeof(command));
                for (int i = 0; i < MAX_RETRIES_DEFAULT; i++) {
                    TIME_OUT(read_incomming(fd, response), success);
                    if (success)
                        break;
                    LOG("Trying again in %d seconds\n", RETRY_INTERVAL_SEC);
                    sleep(RETRY_INTERVAL_SEC);
                }

                if (!success) {
                    ERROR("Failed to terminate connection on receiver end\n");
                    return;
                }

                // TODO: Implementar isto melhor
                write(fd, acknowledge, acknowledge);
                return;
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        INFO("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    (void)signal(SIGALRM, alarm_handler);

    struct termios oldtio;

    int fd = serial_open(argv[1]);
    serial_config(fd, &oldtio);

    p_phase_handler(fd);

    serial_close(fd, &oldtio);
    INFO("Serial connection closed\n");
    return 0;
}