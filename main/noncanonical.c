/*Non-Canonical Input Processing*/
#include <fcntl.h>
#include <signal.h>
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
    termination,
} p_phases;

void p_phase_handler(int fd) {
    p_phases current = establishment;

    while (true) {
        switch (current) {
            case establishment: {
                uchar command[] = SET(A1);
                uchar response[] = UA(A1);

                if (read_incomming(fd, command)) {
                    LOG("Connection Established\nStarting Data Transfer\n");
                    write(fd, response, sizeof response);
                    current = data_transfer;
                }
                break;
            }

            // TODO: Implementar frames de informação e bit de-stufffing
            case data_transfer: {
                uchar term_command[] = DISC(A1);

                if (read_incomming(fd, term_command)) {
                    INFO("Data Transfer Complete\n");
                    current = termination;
                }
                break;
            }

            case termination: {
                uchar response[] = DISC(A3);
                uchar acknowledge[] = UA(A3);
                bool success = false;

                write(fd, response, sizeof response);
                TIME_OUT_AFTER_N_TRIES(read_incomming(fd, acknowledge), MAX_RETRIES_DEFAULT, success);
                if (!success) {
                    ERROR("Failed to received disconnect acknowledgemnet\n");
                    return;
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    (void)signal(SIGALRM, alarm_handler);

    struct termios oldtio;
    int fd = serial_open(argv[1]);
    serial_config(fd, &oldtio);
    INFO("New termios structure set.\n");

    p_phase_handler(fd);

    serial_close(fd, &oldtio);
    INFO("Serial connection closed\n");
    return 0;
}
