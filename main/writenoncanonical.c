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
#define RETRY_INTERVAL_SEC 3

bool alarm_flag = false;

#define TIME_OUT_AFTER_N_TRIES(FUNC, N_TRIES, RET_BOOL)                  \
    {                                                                    \
        RET_BOOL = false;                                                \
        for (int i = 0; i < N_TRIES; i++) {                              \
            alarm_flag = false;                                          \
            alarm(ALARM_TIMEOUT_SEC);                                    \
            while (!alarm_flag) {                                        \
                if (FUNC) {                                              \
                    alarm(0);                                            \
                    RET_BOOL = true;                                     \
                    break;                                               \
                }                                                        \
            }                                                            \
            if (!alarm_flag)                                             \
                break;                                                   \
            ALERT(                                                       \
                "Command timed out.\n\t- Trying again in %d seconds.\n", \
                RETRY_INTERVAL_SEC);                                     \
            sleep(RETRY_INTERVAL_SEC);                                   \
        }                                                                \
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

void p_phase_handler(int fd) {
    p_phases current = establishment;

    while (true) {
        switch (current) {
            case establishment: {
                uchar command[] = SET(A1);
                uchar response[] = UA(A1);
                bool success = false;

                write(fd, command, sizeof command);
                TIME_OUT_AFTER_N_TRIES(read_incomming(fd, response), MAX_RETRIES_DEFAULT, success);
                if (!success) {
                    ERROR("Failed to establish serial port connection.\n");
                    return;
                }
                INFO("Serial port connection established.\n");
                current = data_transfer;
                break;
            }

            // TODO: implementar tramas de informação e bit stuffing
            case data_transfer: {
                // write_from_kb(fd);
                current = termination;
                break;
            }

            case termination: {
                uchar command[] = DISC(A1);
                uchar response[] = DISC(A3);
                uchar acknowledge[] = UA(A3);
                bool success = false;

                write(fd, command, sizeof(command));
                TIME_OUT_AFTER_N_TRIES(read_incomming(fd, response), MAX_RETRIES_DEFAULT, success);
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