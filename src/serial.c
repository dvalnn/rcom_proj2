#include "serial.h"

int serial_open(char* serial_port) {
    int fd;

    fd = open(serial_port, O_RDWR | O_NOCTTY);

    if (fd < 0) {
        ERROR("%s", serial_port);
        exit(-1);
    }

    return fd;
}

void serial_config(int fd, struct termios* oldtio) {
    struct termios newtio;

    LOG("SAVING terminal settings\n");

    sleep(1);
    if (tcgetattr(fd, oldtio) == -1) { /* save current port settings */
        perror("tcgetattr");
        exit(-1);
    }

    LOG("Settings saved\n");

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    //* VTIME - Timeout in deciseconds for noncanonical read.
    //* VMIN - Minimum number of characters for noncanonical read.
    // VTIME = 1 para esperar 100 ms por read
    newtio.c_cc[VTIME] = 1;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        ERROR("tcsetattr");
        exit(-1);
    }

    LOG("New termios structure set.\n");
}

void serial_close(int fd, struct termios* configs) {
    if (tcsetattr(fd, TCSANOW, configs) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
}