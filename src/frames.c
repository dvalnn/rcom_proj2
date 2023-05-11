#include "frames.h"
#include "log.h"

#include <fcntl.h>
#include <sys/stat.h>

#define BUF_SIZE 255

typedef enum su_states {
    st_START,
    st_FLAG_RCV,
    st_A_RCV,
    st_C_RCV,
    st_BCC1_OK,
    st_STOP
} su_states;

su_states header_state_machine(su_states cur_state, uchar rcved, uchar* msg_type) {
    su_states new_state = cur_state;

    switch (cur_state) {
        case st_START:
            ALERT("\t>Expected: 0x%.02x; Received 0x%.02x\n", msg_type[0], (unsigned int)(rcved & 0xFF));
            if (rcved == msg_type[0])
                new_state = st_FLAG_RCV;
            break;

        case st_FLAG_RCV:
            ALERT("\t>Expected: 0x%.02x; Received 0x%.02x\n", msg_type[1], (unsigned int)(rcved & 0xFF));
            if (rcved == msg_type[1])
                new_state = st_A_RCV;
            else if (rcved != msg_type[0])
                new_state = st_START;
            break;

        case st_A_RCV:
            ALERT("\t>Expected: 0x%.02x; Received 0x%.02x\n", msg_type[2], (unsigned int)(rcved & 0xFF));
            if (rcved == msg_type[2])
                new_state = st_C_RCV;
            else if (rcved == msg_type[0])
                new_state = st_FLAG_RCV;
            else
                new_state = st_START;
            break;

        case st_C_RCV:
            ALERT("\t>Expected: 0x%.02x; Received 0x%.02x\n", msg_type[3], (unsigned int)(rcved & 0xFF));
            if (rcved == msg_type[3])
                new_state = st_BCC1_OK;
            else if (rcved == msg_type[0])
                new_state = st_FLAG_RCV;
            else
                new_state = st_START;
            break;

        case st_BCC1_OK:
            ALERT("\t>Expected: 0x%.02x; Received 0x%.02x\n", msg_type[4], (unsigned int)(rcved & 0xFF));
            if (rcved == msg_type[4])
                new_state = st_STOP;
            else
                new_state = st_START;
            break;

        case st_STOP:
            break;
    }

    return new_state;
}

int verify_header(unsigned char* message, int message_length, uchar* msg_type, int is_info) {
    LOG("%s\n\t>%d chars received\n", message, message_length);
    su_states cur_state = st_START;

    for (int i = 0; i < message_length; i++) {
        ALERT("\tChar received: 0x%.02x (position %d)\n", (unsigned int)(message[i] & 0xff), i);
        cur_state = header_state_machine(cur_state, message[i], msg_type);
        ALERT("\tCurrent state: %d\n", cur_state);
        if (cur_state == st_STOP)
            return 1;
        if (cur_state == st_BCC1_OK && is_info)
            return 1;
    }

    return 0;
}

int read_incomming(int fd, uchar* msg_type) {
    unsigned char buf[BUF_SIZE];
    int length = read(fd, buf, sizeof(buf));
    if (length) {
        LOG("Received %d characters\n", length);
        return verify_header(buf, length, msg_type, 0);
    }
    return 0;
}

int verify_tail(uchar* buf, int length) {
    // TODO: descobrir o que raio é para fazer c/ o BCC2
    return buf[length - 2] == F;
}

int read_info(int fd, int id) {
    unsigned char buf[BUF_SIZE];
    int length = read(fd, buf, sizeof buf);

    if (!length)
        return 0;

    // TODO: fazer de-stuffing

    uchar disconnect[] = DISC(A1);
    uchar info[] = INFO_MSG(A1, id);
    if (verify_header(buf, length, disconnect, 0)) {
        LOG("Received disconnect\n");
        return 2;
    }

    if (verify_header(buf, length, info, 1) && verify_tail(buf, length)) {
        int file = creat("output.txt", O_WRONLY | O_CREAT | O_APPEND);
        if (!file) {
            ERROR("Output.txt not created\n");
            return -2;
        }

        LOG("%s\n", buf);

        uchar* start = &buf[4];
        buf[length - 2] = '\0';

        write(file, start, length);
        close(file);
        return 1;
    }

    return -1;
}

#undef BUF_SIZE