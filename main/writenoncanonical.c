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
#include <unistd.h>

#include "frames.h"
#include "log.h"
#include "sds.h"
#include "serial.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

#define MAX_RETRIES 3
#define ALARM_TIMEOUT_SEC 3
#define ALARM_SLEEP_SEC 1

#define READ_BUFFER_SIZE 32

bool alarm_flag = false;
int alarm_count = 0;

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

void alarm_handler(int signum)  // atende alarme
{
    alarm_count++;
    alarm_flag = true;
    ALERT("Alarm Interrupt Triggered with code %d - Attempt %d/%d\n", signum, alarm_count, MAX_RETRIES);
    if (alarm_count < MAX_RETRIES) {
        ALERT("Sleeping for %ds before next attempt\n\n", ALARM_SLEEP_SEC);
        sleep(ALARM_SLEEP_SEC);
    }
}

bool send_frame(int fd, sds packet, frame_type ft_expected) {
    uchar rcved;

    frame_type ft_detected = ft_ANY;
    frame_state fs_current = fs_FLAG1;

    bool success = false;

    while (alarm_count < MAX_RETRIES) {
        write(fd, packet, sdslen(packet));

        alarm(ALARM_TIMEOUT_SEC);
        while (true) {
            if (alarm_flag)
                break;

            int nbytes = read(fd, &rcved, sizeof rcved);
            if (!nbytes)
                continue;
            fs_current = frame_handler(fs_current, &ft_detected, rcved);

            if (ft_detected == ft_expected && fs_current == fs_VALID) {
                alarm(0);
                alarm_count = 0;
                success = true;
                break;
            }
        }
        if (success)
            break;

        alarm_flag = false;
    }
    return success;
}

bool llopen(int fd) {
    sds set = sdsnewframe(ft_SET);

    bool success = send_frame(fd, set, ft_UA);

    sdsfree(set);

    if (success)
        INFO("Handshake complete\n");
    else
        ERROR("Handshake failure - check connection\n");

    return success;
}

uchar calculate_bcc2(sds data) {
    uchar bcc2 = data[0];
    for (int i = 1; i < sdslen(data); i++)
        bcc2 = bcc2 ^ data[i];
    return bcc2;
}

bool llwrite(int fd, char* filepath) {
    int file = open(filepath, O_RDONLY);
    if (file == -1) {
        ERROR("Could not open '%s' file\n", filepath);
        return false;
    }

    int id = 0;
    uchar buf[READ_BUFFER_SIZE];

    while (true) {
        int nbytes = read(file, &buf, READ_BUFFER_SIZE);
        if (!nbytes)
            break;

        frame_type ft_expected = id ? ft_RR0 : ft_RR1;
        frame_type ft_format = id ? ft_INFO1 : ft_INFO0;

        //* Create the INFO frame header (without BCC2 and Last Flag)
        sds header = sdsnewframe(ft_format);
        sdsrange(header, 0, -2);

        //* Create data string from buf and calculate bcc2
        sds data = sdsnewlen(buf, nbytes);
        uchar bcc2 = calculate_bcc2(data);
        uchar tail[] = {bcc2, F, '\0'};

        //* Format INFO frame with header and tail and byte-stuff data
        sds data_formated = sdscatsds(header, byte_stuffing(data, true));
        data_formated = sdscat(data_formated, (char*)tail);

        sds data_formated_repr = sdscatrepr(sdsempty(), data_formated, sdslen(data_formated));
        LOG("Formated INFO frame: %s\n", data_formated_repr);
        LOG("Calculated BCC2: 0x%.02x = '%c'\n", (unsigned int)(bcc2 & 0xFF), bcc2);
        send_frame(fd, data_formated, ft_expected);

        id = !id;

        sdsfree(data);
        sdsfree(data_formated);
    }
    close(file);
    return true;
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

    if (!llopen(fd)) {
        serial_close(fd, &oldtio);
        INFO("Serial connection closed\n");
        return -1;
    }

    llwrite(fd, "wywh.txt");
    // llopen -> llwrite -> llread -> llclose
    // TODO: llread/llwrite/llclose

    serial_close(fd, &oldtio);
    INFO("Serial connection closed\n");
    return 0;
}