#include "frames.h"

sds create_frame(frame_type ftype) {
    return sdsnewlen(FFormat[ftype], FRAME_SIZE);
}

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

    ALERT("-------------\n");
    ALERT("Current frame: %s\n", FType_STRING[candidate]);
    ALERT("Current state: %s\n", FState_STRING[cur_state]);
    ALERT("Received char: 0x%.02x = '%c'\n", (unsigned int)(rcved & 0xFF), rcved);

    switch (new_state) {
        case fs_FLAG1:
            candidate = ft_ANY;
            if (rcved == F)
                new_state = fs_A;
            break;

        case fs_A:
            if (rcved == A1)
                new_state = fs_C;
            else if (rcved != F)
                new_state = fs_FLAG1;
            break;

        case fs_C:
            if (rcved == F) {
                new_state = fs_FLAG1;
                break;
            }

            candidate = control_byte_handler(rcved);

            if (candidate == ft_INVALID)
                new_state = fs_FLAG1;
            else
                new_state = fs_BCC1_OK;
            break;

        case fs_BCC1_OK: {
            if (rcved == F) {
                new_state = fs_FLAG1;
                break;
            }

            bool bcc_ok = bcc1_handler(rcved, candidate);

            if (bcc_ok && !(candidate == ft_INFO0 || candidate == ft_INFO1))
                new_state = fs_FLAG2;
            else if (bcc_ok)
                new_state = fs_INFO;
            else
                new_state = fs_FLAG1;
            break;
        }

        case fs_FLAG2:
            if (rcved == F)
                new_state = fs_VALID;
            else
                new_state = fs_FLAG1;
            break;

        case fs_INFO:
            if (rcved == F)
                new_state = fs_BCC2_OK;
            break;

        case fs_BCC2_OK:
            if (rcved == 1)
                new_state = fs_VALID;
            else {
                candidate = ft_INVALID;
                new_state = fs_FLAG1;
            }
            break;

        case fs_VALID:
            new_state = fs_FLAG1;
            candidate = ft_ANY;
            break;
    }

    ALERT("Next frame: %s\n", FType_STRING[candidate]);
    ALERT("Next state: %s\n", FState_STRING[new_state]);
    ALERT("-------------\n\n");
    *ftype = candidate;
    return new_state;
}

/**
 * @brief byte stuffing and destuffing funtion
 *
 * @param input sds string to stuff / destuff
 * @param stuff_string true for stuffing operation, false for destuffing
 * @return sds
 */
sds byte_stuffing(sds input, bool stuff_string) {
    int ntokens;

    char split_token1[] = {ESC};
    char join_token1[] = {ESC, ESC_SEQ(ESC)};

    char split_token2[] = {F};
    char join_token2[] = {ESC, ESC_SEQ(F)};

    char* split_token = stuff_string ? split_token1 : join_token1;
    int split_size = stuff_string ? sizeof split_token1 : sizeof join_token1;

    char* join_token = stuff_string ? join_token1 : split_token1;
    int join_size = stuff_string ? sizeof join_token1 : sizeof split_token1;

    sds* tokens = sdssplitlen(input, sdslen(input), split_token1, split_size, &ntokens);
    sds pass1 = sdsjoinsds(tokens, ntokens, join_token1, sizeof join_token1);

    sdsfreesplitres(tokens, ntokens);

    split_token = stuff_string ? split_token2 : join_token2;
    split_size = stuff_string ? sizeof split_token2 : sizeof join_token2;

    join_token = stuff_string ? join_token2 : split_token2;
    join_size = stuff_string ? sizeof join_token2 : sizeof split_token2;

    tokens = sdssplitlen(pass1, sdslen(pass1), split_token, split_size, &ntokens);
    sds result = sdsjoinsds(tokens, ntokens, join_token, join_size);

    sdsfreesplitres(tokens, ntokens);
    sdsfree(pass1);

    return result;
}

/**
 * @brief obsolete
 *
 * @param input
 * @return uchar
 */
uchar byte_destuffing(uchar input) {
    if (input == ESC)
        return 1;
    if (input == ESC_SEQ(F))
        return F;
    if (input == ESC_SEQ(ESC))
        return ESC;

    return 0;
}

/**
 * @brief obsolete
 *
 * @param fd
 * @return uchar
 */
uchar read_byte(int fd) {
    uchar byte1;
    uchar byte2;

    int b_read = read(fd, &byte1, sizeof byte1);
    if (!b_read)
        return 0;

    ALERT("Received 0x%.02x\n", (unsigned int)(byte1 & 0xFF));

    if (byte_destuffing(byte1) != 1)
        return byte1;

    ALERT("Destuffing candidate - reading another byte\n");

    b_read = read(fd, &byte2, sizeof byte2);
    uchar res = byte_destuffing(byte2);

    ALERT("Received 0x%.02x\n", (unsigned int)(byte2 & 0xFF));

    if (!b_read || !res || res == 1) {
        lseek(fd, -1, SEEK_CUR);
        ALERT("\t> False Alarm - char rolled back\n");
        return byte1;
    }

    ALERT("Destuffed 0x%.02x0x%.02x into 0x%.02x\n",
          (unsigned int)(byte1 & 0xFF),
          (unsigned int)(byte2 & 0xFF),
          (unsigned int)(res & 0xFF));

    return res;
}
