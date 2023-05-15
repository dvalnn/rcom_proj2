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
    ALERT("Alarm Interrupt Triggered with code %d\n", signum);
    if (alarm_count < MAX_RETRIES) {
        ALERT("Sleeping for %ds before next retry\n\n", ALARM_SLEEP_SEC);
        sleep(ALARM_SLEEP_SEC);
    }
}

bool llopen(int fd) {
    uchar rcved;

    frame_type ft_detected = ft_ANY, f_expected = ft_UA;
    frame_state fs_current = fs_START;

    bool success = false;

    uchar format_buf[] = SET;

    sds set = sdsnewlen(format_buf, sizeof format_buf);
    sds set_stuffed = byte_stuffing(set);
    sds set_stuffed_repr = sdsempty();

    set_stuffed_repr = sdscatrepr(set_stuffed_repr, set_stuffed, sdslen(set_stuffed));

    alarm_count = 0;

    while (true) {
        LOG("Sending SET Message (stuffed) %s\n", set_stuffed_repr);
        if (write(fd, set_stuffed, sdslen(set_stuffed)) == -1) {
            ERROR("Write failes -- check connection\n");
        }
        LOG("Expecting UA response (Timing out in %ds, try number %d/%d)\n", ALARM_TIMEOUT_SEC, alarm_count + 1, MAX_RETRIES);

        alarm(ALARM_TIMEOUT_SEC);
        while (true) {
            if (alarm_flag)
                break;

            rcved = read_byte(fd);
            if (!rcved)
                continue;

            fs_current = frame_handler(fs_current, &ft_detected, rcved);

            if (ft_detected == f_expected && fs_current == fs_VALID) {
                alarm(0);
                success = true;
                break;
            }
        }

        if (success)
            break;

        alarm_flag = false;
        if (alarm_count == MAX_RETRIES) {
            alarm_count = 0;
            break;
        }
    }

    if (success)
        INFO("Handshake complete\n");
    else
        ERROR("Handshake failure - check connection\n");

    sdsfree(set);
    sdsfree(set_stuffed);
    sdsfree(set_stuffed_repr);

    return success;
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
    // llopen -> llwrite -> llread -> llclose
    // TODO llread/llwrite/llclose

    serial_close(fd, &oldtio);
    INFO("Serial connection closed\n");
    return 0;
}

/*
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

    LOG("%s\n", buf);

    for (int i = 0; i < sizeof header; i++)
        msg_buf[i] = header[i];
    for (int i = sizeof header; i < size; i++)
        msg_buf[i] = buf[i - (sizeof header)];

    msg_buf[size - 2] = F;
    msg_buf[size - 1] = F;
    // msg_buf[size - 1] = '\0';

    LOG("%s\n", msg_buf);

    write(fd, msg_buf, sizeof msg_buf);
}

void send_info(int fd) {
    int file = open("wywh.txt", O_RDONLY);  //! :Blushed:
    if (!file) {
        ERROR("Failed to open file 'wywh.txt'\n");
        return;
    }
    lseek(file, SEEK_SET, 0);

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
            sleep(RETRY_INTERVAL_SEC);
            TIME_OUT(read_incomming(fd, response), success);
            if (success)
                break;
            if (i < MAX_RETRIES_DEFAULT - 1) {
                LOG("Retrying in %d seconds\n", RETRY_INTERVAL_SEC);
            }
        }

        if (!success)
            break;

        id = !id;
    }
    lseek(file, SEEK_SET, 0);
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
                    // sleep(RETRY_INTERVAL_SEC);
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

                for (int i = 0; i < MAX_RETRIES_DEFAULT; i++) {
                    write(fd, command, sizeof(command));
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
                write(fd, acknowledge, sizeof acknowledge);
                return;
            }
        }
    }
}
*/