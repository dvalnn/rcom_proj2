#include "frames.h"
#include "log.h"

#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>

frame_type control_byte_handler(uchar byte) {
    switch (byte) {
        case C_SET:
            return ft_SET;
        case C_UA:
            return ft_UA;
        case C_RR(0):
            return ft_RR0;
        case C_RR(1):
            return ft_RR1;
        case C_REJ(0):
            return ft_REJ0;
        case C_REJ(1):
            return ft_REJ1;
        case C_NS(0):
            return ft_INFO0;
        case C_NS(1):
            return ft_INFO1;

        default:
            return ft_INVALID;
    }
}

bool bcc1_handler(uchar byte, frame_type ftype) {
    switch (ftype) {
        case ft_SET:
            return byte == (A1 ^ C_SET);

        case ft_UA:
            return byte == (A1 ^ C_UA);
        case ft_DISC:
            return byte == (A1 ^ C_DISC);

        case ft_RR0:
            return byte == (A1 ^ C_RR(0));

        case ft_RR1:
            return byte == (A1 ^ C_RR(1));

        case ft_REJ0:
            return byte == (A1 ^ C_REJ(0));

        case ft_REJ1:
            return byte == (A1 ^ C_REJ(1));

        case ft_INFO0:
            return byte == (A1 ^ C_NS(0));

        case ft_INFO1:
            return byte == (A1 ^ C_NS(1));

        default:
            return false;
    }
}

frame_state frame_handler(frame_state cur_state, frame_type* ftype, uchar rcved) {
    frame_state new_state = cur_state;
    frame_type candidate = *ftype;

    ALERT("Received 0x%.02x\n", (unsigned int)(rcved & 0xFF));
    ALERT("Candidate frame: %d\n", candidate);

    switch (new_state) {
        case fs_START:
            if (rcved == F)
                new_state = fs_FLAG1;
            break;

        case fs_FLAG1:
            if (rcved == A1)
                new_state = fs_A;
            else if (rcved != F)
                new_state = fs_START;
            break;

        case fs_A:
            if (rcved == F) {
                new_state = fs_FLAG1;
                break;
            }

            candidate = control_byte_handler(rcved);

            if (candidate == ft_INVALID)
                new_state = fs_START;
            else
                new_state = fs_C;
            break;

        case fs_C:
            if (rcved == F) {
                new_state = fs_FLAG1;
                break;
            }
            if (bcc1_handler(rcved, candidate))
                new_state = fs_BCC1_OK;
            else {
                new_state = fs_START;
                candidate = ft_INVALID;
            }
            break;

        case fs_BCC1_OK:
            if (rcved == F && !(candidate == ft_INFO0 || candidate == ft_INFO1)) {
                new_state = fs_VALID;
            } else if ((candidate == ft_INFO0 || candidate == ft_INFO1))
                new_state = fs_INFO;
            else {
                new_state = fs_START;
                candidate = ft_INVALID;
            }
            break;

        case fs_INFO:
            if (rcved == F)
                new_state = fs_VALID;
            break;

        case fs_BCC2_OK:
            // TODO: Handler p/ bcc2
            break;

        case fs_VALID:
            break;
    }

    *ftype = candidate;
    return new_state;
}
/*
int verify_header(unsigned char* message, int message_length, uchar* msg_type, int is_info) {
    LOG("%s\n\t>%d chars received\n", message, message_length);
    frame_state cur_state = frame_START;

    for (int i = 0; i < message_length; i++) {
        ALERT("\tChar received: 0x%.02x (position %d)\n", (unsigned int)(message[i] & 0xff), i);
        cur_state = frame_handler(cur_state, message[i], msg_type);
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
*/
