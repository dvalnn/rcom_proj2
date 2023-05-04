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

#define MAX_RETRIES 5
#define ALARM_TIMEOUT_SEC 3
#define RETRY_INTERVAL_SEC 3

bool alarm_flag = false;

typedef enum p_phases {
    establishment,
    data_transfer,
    termination,
} p_phases;

void on_alarm()  // atende alarme
{
    ALARM("Alarm Interrupt Triggered\n");
    alarm_flag = true;
}

bool send_command(int fd, uchar* command, int clen, uchar* response) {
    for (int tries = 1; tries < MAX_RETRIES; tries++) {
        LOG("Sending command.\n\t- Attempt number: %d\n", tries);
        write(fd, command, clen);
        if (!response)
            return true;

        alarm(ALARM_TIMEOUT_SEC);
        while (!alarm_flag) {
            if (read_incomming(fd, response)) {
                alarm(0);
                return true;
            }
        }

        if (!alarm_flag)
            break;

        ALARM("Command timed out.\n\t- Trying again in 3 seconds.\n");
        alarm_flag = false;
        sleep(RETRY_INTERVAL_SEC);
    }

    return false;
}

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
                uchar command[] = DISC(A1);

                if (read_incomming(fd, command)) {
                    INFO("Data Transfer Complete\n");
                    current = termination;
                }
                break;
            }

            case termination: {
                uchar response[] = DISC(A3);
                uchar acknowledge[] = UA(A3);

                if (!send_command(fd, response, sizeof response, acknowledge)) {
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

    (void)signal(SIGALRM, on_alarm);

    struct termios oldtio;
    int fd = serial_open(argv[1]);
    serial_config(fd, &oldtio);
    INFO("New termios structure set.\n");

    p_phase_handler(fd);

    serial_close(fd, &oldtio);
    INFO("Serial connection closed\n");
    return 0;
}
