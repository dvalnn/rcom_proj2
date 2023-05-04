#include "frames.h"
#include "log.h"

#define BUF_SIZE 255

typedef enum su_states {
    st_START,
    st_FLAG_RCV,
    st_A_RCV,
    st_C_RCV,
    st_BCC1_OK,
    st_STOP
} su_states;

su_states state_update(su_states cur_state, uchar rcved, uchar* msg_type) {
    su_states new_state = cur_state;

    switch (cur_state) {
        case st_START:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", msg_type[0], (unsigned int)(rcved & 0xFF));
            if (rcved == msg_type[0])
                new_state = st_FLAG_RCV;
            break;

        case st_FLAG_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", msg_type[1], (unsigned int)(rcved & 0xFF));
            if (rcved == msg_type[1])
                new_state = st_A_RCV;
            else if (rcved != msg_type[0])
                new_state = st_START;
            break;

        case st_A_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", msg_type[2], (unsigned int)(rcved & 0xFF));
            if (rcved == msg_type[2])
                new_state = st_C_RCV;
            else if (rcved == msg_type[0])
                new_state = st_FLAG_RCV;
            else
                new_state = st_START;
            break;

        case st_C_RCV:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", msg_type[3], (unsigned int)(rcved & 0xFF));
            if (rcved == msg_type[3])
                new_state = st_BCC1_OK;
            else if (rcved == msg_type[0])
                new_state = st_FLAG_RCV;
            else
                new_state = st_START;
            break;

        case st_BCC1_OK:
            ALARM("\t>Expected: 0x%.02x; Received 0x%.02x\n", msg_type[4], (unsigned int)(rcved & 0xFF));
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

int parse_message(unsigned char* message, int message_length, uchar* msg_type) {
    INFO("%s\n\t>%d chars received\n", message, message_length);

    su_states cur_state = st_START;
    for (int i = 0; i < message_length; i++) {
        ALARM("\tChar received: 0x%.02x (position %d)\n", (unsigned int)(message[i] & 0xff), i);
        cur_state = state_update(cur_state, message[i], msg_type);
        ALARM("\tCurrent state: %d\n", cur_state);
        if (cur_state == st_STOP)
            break;
    }

    return cur_state == st_STOP;
}

int read_incomming(int fd, uchar* msg_type) {
    unsigned char buf[BUF_SIZE];
    int lenght = read(fd, buf, sizeof(buf));
    if (lenght) {
        LOG("Received %d characters\n", lenght);
        return parse_message(buf, lenght, msg_type);
    }
    return 0;
}

#undef BUF_SIZE