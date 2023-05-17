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
#include <unistd.h>

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

void write_to_file(sds data, char* filename) {
    int file = open(filename, O_WRONLY | O_CREAT);
    write(file, data, sdslen(data));
    close(file);
}

uchar validate_bcc2(sds data) {
    uchar bcc2 = data[0];
    uchar bcc2_expected = data[sdslen(data) - 1];

    for (int i = 1; i < sdslen(data) - 1; i++)
        bcc2 = bcc2 ^ data[i];

    return bcc2 == bcc2_expected;
}

void receiver(int fd) {
    uchar rcved;

    frame_type frame_atual = ft_ANY;
    frame_state estado_atual = fs_START;

    sds ua = sdsnewframe(ft_UA);
    sds rr0 = sdsnewframe(ft_RR0);
    sds rr1 = sdsnewframe(ft_RR1);
    sds disc = sdsnewframe(ft_DISC);

    sds info_buf = sdsnewlen(sdsempty(), READ_BUFFER_SIZE);
    sds info_destuffed = sdsempty();
    int buf_len = 0;

    bool close = false;

    while (true) {
        int nbytes = read(fd, &rcved, sizeof rcved);
        if (!nbytes)
            continue;

        estado_atual = frame_handler(estado_atual, &frame_atual, rcved);

        switch (estado_atual) {
            case fs_START:
                sdsclear(info_buf);
                break;

            case fs_INFO:
                info_buf[buf_len] = rcved;
                buf_len++;
                break;

            case fs_BCC2_OK:
                info_buf = sdsRemoveFreeSpace(info_buf);
                info_destuffed = byte_stuffing(info_buf, false);
                estado_atual = frame_handler(estado_atual, &frame_atual, validate_bcc2(info_destuffed));
                sdsrange(info_buf, 0, -2);
                buf_len = 0;
                break;

            case fs_VALID:
                switch (frame_atual) {
                    case ft_SET:
                        write(fd, ua, sdslen(ua));
                        break;

                    case ft_UA:
                        INFO("Received Last Flag. Closing connection.\n");
                        close = true;
                        break;

                    case ft_DISC:
                        write(fd, disc, sdslen(disc));
                        break;

                    case ft_INFO0:
                        write_to_file(info_buf, "output.txt");
                        write(fd, rr1, sdslen(rr1));
                        break;

                    case ft_INFO1:
                        write_to_file(info_buf, "output.txt");
                        write(fd, rr0, sdslen(rr0));
                        break;

                    default:
                        close = true;
                        break;
                }
                // reset the state machine
                frame_atual = frame_handler(frame_atual, &frame_atual, 0);
                break;

            default:
                break;
        }

        if (buf_len > sdslen(info_buf))
            sdsgrowzero(info_buf, sdslen(info_buf) + READ_BUFFER_SIZE);

        if (close)
            break;
    }

    sdsfree(ua);
    sdsfree(rr0);
    sdsfree(rr1);
    sdsfree(disc);
    sdsfree(info_buf);
}  // TODO -> Fazer para enviar

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
