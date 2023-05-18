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
        case C_DISC:
            return ft_DISC;
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
/*
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
*/

// write a fucntion to stuff a byte stream.
// ESC -> ESC ESC_SEQ(ESC)
// F -> ESC ESC_SEQ(F)
sds byte_stuffing_alt(sds input_data) {
    sds output_data = sdsempty();

    uchar escape = ESC;
    uchar escaped_escape = ESC_SEQ(ESC);

    uchar flag = F;
    uchar escaped_flag = ESC_SEQ(F);

    uchar zero = 0;
    uchar escaped_zero = ESC_SEQ(0);

    for (int i = 0; i < sdslen(input_data); i++) {
        if (input_data[i] == ESC) {
            output_data = sdscatlen(output_data, &escape, 1);
            output_data = sdscatlen(output_data, &escaped_escape, 1);
        } else if (input_data[i] == flag) {
            output_data = sdscatlen(output_data, &escape, 1);
            output_data = sdscatlen(output_data, &escaped_flag, 1);
        } else if (input_data[i] == zero) {
            output_data = sdscatlen(output_data, &escape, 1);
            output_data = sdscatlen(output_data, &escaped_zero, 1);
        } else {
            output_data = sdscatlen(output_data, &input_data[i], 1);
        }
    }
    return output_data;
}

// write a fucntion the destuff a byte stream that was stuffed with the previous function
sds byte_destuffing(sds input_data) {
    sds output_data = sdsempty();

    uchar escape = ESC;
    uchar escaped_escape = ESC_SEQ(ESC);

    uchar flag = F;
    uchar escaped_flag = ESC_SEQ(F);

    uchar zero = 0;
    uchar escaped_zero = ESC_SEQ(0);

    for (int i = 0; i < sdslen(input_data); i++) {
        if (input_data[i] == escape) {
            if (input_data[i + 1] == escaped_escape) {
                output_data = sdscatlen(output_data, &escape, 1);
                i++;
            } else if (input_data[i + 1] == escaped_flag) {
                output_data = sdscatlen(output_data, &flag, 1);
                i++;
            } else if (input_data[i + 1] == escaped_zero) {
                output_data = sdscatlen(output_data, &zero, 1);
                i++;
            } else
                output_data = sdscatlen(output_data, &input_data[i], 1);

        } else {
            output_data = sdscatlen(output_data, &input_data[i], 1);
        }
    }
    return output_data;
}
