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

bool send_frame(int fd, sds packet, frame_type ft_expected) {
    uchar rcved;

    frame_type ft_detected = ft_ANY;
    frame_state fs_current = fs_START;

    bool success = false;

    while (alarm_count < MAX_RETRIES) {
        write(fd, packet, sdslen(packet));

        alarm(ALARM_TIMEOUT_SEC);
        while (true) {
            if (alarm_flag)
                break;

            int nbytes = read(fd, &rcved, sizeof rcved);
            fs_current = frame_handler(fs_current, &ft_detected, rcved);

            if (ft_detected == ft_expected && fs_current == fs_VALID) {
                alarm(0);
                success = true;
                break;
            }
        }
        if (success)
            break;

        alarm_flag = false;
    }

    alarm_count = 0;
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
        sdsrange(header, 0, -3);

        //* Create data string from buf and calculate bcc2
        sds data = sdsnewlen(buf, nbytes);
        uchar bcc2 = calculate_bcc2(data);
        uchar tail[] = {bcc2, F, '\0'};

        //* Format INFO frame with header and tail and byte-stuff data
        sds data_formated = sdscatsds(header, byte_stuffing(data));
        data_formated = sdscat(data_formated, (char*)tail);

        sds data_formated_repr = sdscatrepr(sdsempty(), data_formated, sdslen(data_formated));
        LOG("Formated INFO frame: %s\n", data_formated_repr);

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