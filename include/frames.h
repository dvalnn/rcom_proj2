#ifndef _SU_FRAMES_H_
#define _SU_FRAMES_H_

#define F 0x5C
#define ESC 0x5d
#define ESC_SEQ (OCT)(OCT ^ 0x20)

#define A1 0x01  //* For commands sent by the Transmitter and Answers sent by the Receiver
#define A3 0x03  //* For commands sent by the Receiver and Answers sent by the Transmitter

#define C_SET 0b00000011
#define C_DISC 0b00001011
#define C_UA 0b00000111
#define C_RR(X) ((X << 5) | 0b00000001)
#define C_REJ(X) ((X << 5) | 0b00000101)
#define C_NS(X) ((X << 1) & 0b00000010)

#define SET \
    { F, A1, C_SET, A1 ^ C_SET, F }

#define DISC \
    { F, A1, C_DISC, A1 ^ C_DISC, F }

#define UA \
    { F, A1, C_UA, A1 ^ C_UA, F }

#define RR(X) \
    { F, A1, C_RR(X), A1 ^ C_RR(X), F }

#define REJ(X) \
    { F, A1, C_REJ(X), A1 ^ C_REJ(X), F }

#define INFO_MSG(X) \
    { F, A1, C_NS(X), A1 ^ C_NS(X), F }

typedef unsigned char uchar;

typedef enum _frame_type {
    INVALID_FRAME,
    SET_FRAME,
    UA_FRAME,
    DISC_FRAME,
    RR0_FRAME,
    RR1_FRAME,
    REJ0_FRAME,
    REJ1_FRAME,
    INFO0_FRAME,
    INFO1_FRAME
} frame_type;

typedef enum _frame_state {
    frame_START,
    frame_FLAG1,
    frame_A,
    frame_C,
    frame_BCC1_OK,
    frame_INFO,
    frame_BCC2_OK,
    frame_VALID,
} frame_state;

frame_state frame_handler(frame_state cur_state, frame_type* ftype, uchar rcved);

// int read_incomming(int fd, uchar* msg_type);
// int read_info(int fd, int id);

#endif  // _SU_FRAMES_H