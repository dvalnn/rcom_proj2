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

#include "log.h"
#include "serial.h"
#include "suFrames.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */

#define MAX_RETRIES 5

bool alarm_flag = false;

void on_alarm();  // atende alarme
void set_connection(int fd);

bool establish_connection(int fd) {
    uchar m_type[] = UA(A1);

    for (int tries = 1; tries < MAX_RETRIES; tries++) {
        LOG("Attemping SET/UA connection.\n\t- Attempt number: %d\n", tries);
        set_connection(fd);

        alarm(3);

        while (!alarm_flag) {
            if (read_incomming(fd, m_type)) {
                alarm(0);
                return true;
            }
        }

        if (!alarm_flag)
            break;

        ALARM("SET/UA Connection failed.\n\t- Trying again in 2 seconds.\n");
        alarm_flag = false;
        sleep(2);
    }

    return false;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        INFO("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    (void)signal(SIGALRM, on_alarm);

    struct termios oldtio;

    int fd = serial_open(argv[1]);
    serial_config(fd, &oldtio);

    if (!establish_connection(fd)) {
        INFO("Failed to establish serial port connection.\n");
        exit(-1);
    }

    INFO("SET/UA successfull.\n");

    // INFO("Waiting for user input. [max 255 chars]\n");

    // int bytes_read = 0;
    // char buf[BUF_SIZE];
    // while (scanf(" %254[^\n]%n", buf, &bytes_read) == 1) {
    //     if (bytes_read > 0) {
    //         int bytes_written = write(fd, buf, bytes_read);
    //         LOG("\t>%d bytes sent\n", bytes_written);

    //         if (buf[0] == 'z' && bytes_written <= 2) {
    //             break;
    //         }
    //         bytes_read = read(fd, buf, bytes_written);  // read receiver echo message
    //         LOG("\t>Echo received (%d chars): %s\n", bytes_read, buf);
    //     }
    // }

    serial_close(fd, &oldtio);

    return 0;
}

void on_alarm()  // atende alarme
{
    ALARM("Alarm Interrupt Triggered\n");
    alarm_flag = true;
}

void set_connection(int fd) {
    uchar set[] = SET(A1);
    write(fd, set, sizeof(set));
}
