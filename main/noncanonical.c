/*Non-Canonical Input Processing*/

#include <fcntl.h>
// #include <signal.h>
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

void ua_message(int fd) {
    uchar ua[] = UA(A1);
    write(fd, ua, sizeof ua);
}

void on_alarm()  // atende alarme
{
    ALARM("Alarm Interrupt Triggered\n");
    // alarm_flag = 1;
    // alarm_counter++;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
        exit(1);
    }

    //(void)signal(SIGALRM, on_alarm);

    struct termios oldtio;
    int fd = serial_open(argv[1]);
    serial_config(fd, &oldtio);

    INFO("New termios structure set.\n");

    LOG("Awaiting SET/UA connection.\n");

    uchar m_type[] = SET(A1);
    while (true) {
        if (read_incomming(fd, m_type)) {
            LOG("SET successfull\n");
            ua_message(fd);
            break;
        }
    }

    // while (STOP == FALSE) {       /* loop for input */
    //     res = read(fd, buf, 255); /* returns after 255 chars have been input */
    //     buf[res] = '\0';          /* so we can printf... */
    //     INFO("%s\n\t>%d chars received\n", buf, res);
    //     if (buf[0] == 'z' && res <= 2)
    //         STOP = TRUE;
    //     LOG("\techoing back... ");
    //     res = write(fd, buf, res);  // echoes back received message
    //     LOG("%d chars sent\n", res);
    // }

    serial_close(fd, &oldtio);

    return 0;
}
