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

// bool alarm_flag = false;

// void alarm_handler(int signum)  // atende alarme
// {
//     ALERT("Alarm Interrupt Triggered with code %d\n", signum);
//     alarm_flag = true;
// }

uchar validate_bcc2(sds data) {
    int bcc2_pos = (int)sdslen(data) - 1;
    LOG("---------------\n");
    LOG("Validating BCC2\n");
    uchar bcc2 = data[0];
    uchar bcc2_expected = data[bcc2_pos];

    // LOG("POS 0: 0x%.02x = '%c'\n", (unsigned int)(bcc2 & 0xFF), bcc2);
    for (int i = 1; i < bcc2_pos; i++) {
        // LOG("POS %d: 0x%.02x = '%c'\n", i, (unsigned int)(data[i] & 0xFF), data[i]);
        bcc2 = bcc2 ^ data[i];
    }

    // ALERT("STRING LENGHT: %ld\n", strlen(data));
    LOG("Calculated BCC2: 0x%.02x = '%c'\n", (unsigned int)(bcc2 & 0xFF), bcc2);
    LOG("Expected BCC2: 0x%.02x = '%c'\n", (unsigned int)(bcc2_expected & 0xFF), bcc2_expected);
    return bcc2 == bcc2_expected;
}

bool llread(int fd, int file) {
    uchar rcved;

    frame_type frame_atual = ft_ANY;
    frame_state estado_atual = fs_FLAG1;

    sds ua = sdsnewframe(ft_UA);
    sds rr0 = sdsnewframe(ft_RR0);
    sds rr1 = sdsnewframe(ft_RR1);
    sds disc = sdsnewframe(ft_DISC);

    sds info_buf = sdsempty();
    sds data = sdsempty();

    bool close = false;
    bool terminate_connection = false;

    while (true) {
        if (close)
            break;

        int nbytes = read(fd, &rcved, sizeof rcved);
        if (!nbytes)
            continue;

        estado_atual = frame_handler(estado_atual, &frame_atual, rcved);

        if (frame_atual == ft_INVALID) {
            break;
        }

        if (estado_atual == fs_INFO) {
            // LOG("Adding 0x%.02x = '%c' to buffer\n\n", (unsigned int)(rcved & 0xFF), rcved);
            char buf[] = {rcved, '\0'};
            info_buf = sdscat(info_buf, buf);
            // LOG("Current Buffer: %s\n\n", info_buf);
        }

        if (estado_atual == fs_BCC2_OK) {
            sdsupdatelen(info_buf);
            data = byte_stuffing(info_buf, false);
            //* Remove bcc1 fom the beginning of the buffer.
            sdsrange(data, 1, -1);
            estado_atual = frame_handler(estado_atual, &frame_atual, validate_bcc2(data));
            //* Remove bcc2 from the end of the buffer.
            sdsrange(data, 0, -2);
            sds data_repr = sdscatrepr(sdsempty(), data, sdslen(data));
            INFO("Received data frame: \n\t>>%s\n", data_repr);
            sdsfree(data_repr);
        }

        if (estado_atual == fs_VALID) {
            close = true;
            sds data_repr = sdscatrepr(sdsempty(), data, sdslen(data));
            switch (frame_atual) {
                case ft_SET:
                    write(fd, ua, sdslen(ua));
                    break;

                case ft_UA:
                    INFO("Received Last Flag. Closing connection.\n");
                    terminate_connection = true;
                    break;

                case ft_DISC:
                    write(fd, disc, sdslen(disc));
                    terminate_connection = true;
                    break;

                case ft_INFO0:
                    INFO("Writing data packet to file\n\n");
                    write(file, data, sdslen(data));
                    write(fd, rr1, sdslen(rr1));
                    break;

                case ft_INFO1:
                    INFO("Writing data packet to file\n\n");
                    write(file, data, sdslen(data));
                    write(fd, rr0, sdslen(rr0));
                    break;

                default:
                    break;
            }
        }
    }

    sdsfree(ua);
    sdsfree(rr0);
    sdsfree(rr1);
    sdsfree(disc);
    sdsfree(data);
    sdsfree(info_buf);

    return terminate_connection;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage:\treceiver.out SerialPort OutputFileName\n\tex: receiver.out /dev/ttyS1 output.txt\n");
        exit(1);
    }

    // (void)signal(SIGALRM, alarm_handler);

    struct termios oldtio;
    int fd = serial_open(argv[1]);
    serial_config(fd, &oldtio);
    INFO("New termios structure set.\n");

    // 0666 argument created the file with read/write permissions for all users.
    int file = open(argv[3], O_WRONLY | O_CREAT, 0666);
    while (!llread(fd, file))
        continue;
    close(file);

    serial_close(fd, &oldtio);
    INFO("Serial connection closed\n");
    return 0;
}
