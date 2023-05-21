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

#define READ_BUFFER_SIZE 256

bool alarm_flag = false;
int alarm_counter = 0;

void alarm_handler(int signum)  // atende alarme
{
    alarm_counter++;
    alarm_flag = true;
    ERROR("Alarm Interrupt Triggered with code %d - Attempt %d/%d\n", signum, alarm_counter, MAX_RETRIES);
}

bool send_frame(int fd, sds packet, frame_type ft_expected) {
    uchar rcved;

    frame_type ft_detected = ft_ANY;
    frame_state fs_current = fs_FLAG1;

    bool success = false;

    while (alarm_counter < MAX_RETRIES) {
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
                alarm_counter = 0;
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
        INFO("-----\nHandshake complete\n-----\n\n");
    else
        ERROR("Handshake failure - check connection\n");

    return success;
}

bool llclose(int fd) {
    sds disc = sdsnewframe(ft_DISC);
    bool success = send_frame(fd, disc, ft_DISC);
    sdsfree(disc);

    if (success)
        INFO("Connection complete\n");
    else
        ERROR("Disconnect failure\n");

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

    bool success = false;

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
        uchar tail1[] = {bcc2, '\0'};
        uchar tail2[] = {F, '\0'};

        //* Append bcc2 to data frame
        data = sdscat(data, (char*)tail1);
        //* Byte-stuff date and bcc2
        sds stuffed_data = byte_stuffing(data);
        sds data_formated = sdscatsds(header, stuffed_data);
        //* Append final flag
        data_formated = sdscat(data_formated, (char*)tail2);

        sds data_repr = sdscatrepr(sdsempty(), data, sdslen(data));
        INFO("Sending data frame: \n\t>>%s\n\t>>Lenght: %ld\n", data_repr, sdslen(data));
        LOG("Calculated BCC2: 0x%.02x = '%c'\n\n", (unsigned int)(bcc2 & 0xFF), bcc2);
        success = send_frame(fd, data_formated, ft_expected);
        if (!success)
            break;

        id = !id;

        sdsfree(data);
        sdsfree(data_repr);
        sdsfree(stuffed_data);
        sdsfree(data_formated);
    }

    close(file);
    return success;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        INFO("Usage:\treceiver.out SerialPort InputFileName\n\tex:receiver.out /dev/ttyS1 wywh.txt\n");
        exit(1);
    }

    (void)signal(SIGALRM, alarm_handler);

    struct termios oldtio;

    int fd = serial_open(argv[1]);
    serial_config(fd, &oldtio);

    bool is_open = false;
    bool write_success = false;

    is_open = llopen(fd);

    if (is_open)
        write_success = llwrite(fd, argv[2]);

    if (!write_success)
        ERROR("File transfer failed\n");

    llclose(fd);

    serial_close(fd, &oldtio);
    INFO("Serial connection closed\n");
    return 0;
}