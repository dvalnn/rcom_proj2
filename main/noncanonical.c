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
#include "sds.h"
#include "serial.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

#define MAX_RETRIES_DEFAULT 3
#define ALARM_TIMEOUT_SEC 3
#define RETRY_INTERVAL_SEC 1

#define READ_BUFFER_SIZE 256

// bool alarm_flag = false;
/*
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
*/
// void alarm_handler(int signum)  // atende alarme
// {
//     ALERT("Alarm Interrupt Triggered with code %d\n", signum);
//     alarm_flag = true;
// }

bool receiver(int fd) {
    uchar rcved;

    frame_type frame_atual = INVALID_FRAME, esperado = SET_FRAME;
    frame_state estado_atual = frame_START;

    uchar format[] = UA;
    sds ua_msg = sdsnewlen(format, sizeof(format));

    while (true) {
        int b_read = read(fd, &rcved, sizeof(rcved));

        if (!b_read)
            continue;

        LOG("Estado atual: %d\n", estado_atual);
        LOG("frame atual: %d\n", frame_atual);

        estado_atual = frame_handler(estado_atual, &frame_atual, rcved);

        if (frame_atual == esperado && estado_atual == frame_VALID) {
            write(fd, ua_msg, sdslen(ua_msg));
            break;
        }
    }

    sdsfree(ua_msg);
    return true;
}
/*
typedef enum p_phases {
    establishment,
    data_transfer,
    termination,
} p_phases;

void p_phase_handler(int fd) {
    p_phases current = establishment;
    int id = 0;

    while (true) {
        switch (current) {
            case establishment: {
                uchar command[] = SET(A1);
                uchar response[] = UA(A1);

                if (read_incomming(fd, command)) {
                    LOG("Connection Established\n\tStarting Data Transfer\n");
                    write(fd, response, sizeof response);
                    return;
                    // current = data_transfer;
                }
                break;
            }

            // TODO: Implementar frames de informação e bit de-stufffing
            case data_transfer: {
                int retval = read_info(fd, id);

                switch (retval) {
                    case 2: {
                        uchar term_command[] = DISC(A1);
                        write(fd, term_command, sizeof term_command);
                        current = termination;
                    }

                    case 1: {
                        id = !id;
                        uchar response[] = RR(A1, id);
                        write(fd, response, sizeof response);
                        break;
                    }

                    case 0:
                        break;

                    case -1:
                        uchar rejection[] = REJ(A3, id);
                        write(fd, rejection, sizeof rejection);
                        INFO("Rejecting segment\n");
                        break;

                    case -2:
                        ERROR("Cannot Open Output File\n");
                        return;

                    default:
                        ERROR("Unexpected value for retval\n");
                        return;
                }

                break;
            }

            case termination: {
                uchar response[] = DISC(A3);
                uchar acknowledge[] = UA(A3);
                bool success = false;

                for (int i = 0; i < MAX_RETRIES_DEFAULT; i++) {
                    write(fd, response, sizeof response);
                    sleep(RETRY_INTERVAL_SEC);
                    TIME_OUT(read_incomming(fd, acknowledge), success);
                    if (success)
                        break;
                    LOG("Retrying in %d seconds\n", RETRY_INTERVAL_SEC);
                }

                if (!success) {
                    ERROR("Failed to received disconnect acknowledgemnet\n");
                    return;
                }
            }
        }
    }
}
*/
int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    // (void)signal(SIGALRM, alarm_handler);

    struct termios oldtio;
    int fd = serial_open(argv[1]);
    serial_config(fd, &oldtio);
    INFO("New termios structure set.\n");

    receiver(fd);

    // while (receiver(fd)) {
    //     continue;
    // }
    // handshake_handler(fd);

    // p_phase_handler(fd);

    serial_close(fd, &oldtio);
    INFO("Serial connection closed\n");
    return 0;
}
