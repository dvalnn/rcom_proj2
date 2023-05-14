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

#define FOREACH_STATE(STATE) \
    STATE(fs_START)          \
    STATE(fs_FLAG1)          \
    STATE(fs_A)              \
    STATE(fs_C)              \
    STATE(fs_BCC1_OK)        \
    STATE(fs_INFO)           \
    STATE(fs_BCC2_OK)        \
    STATE(fs_VALID)

#define FOREACH_FRAME(FRAME) \
    FRAME(ft_ANY)            \
    FRAME(ft_INVALID)        \
    FRAME(ft_SET)            \
    FRAME(ft_UA)             \
    FRAME(ft_DISC)           \
    FRAME(ft_RR0)            \
    FRAME(ft_RR1)            \
    FRAME(ft_REJ0)           \
    FRAME(ft_REJ1)           \
    FRAME(ft_INFO0)          \
    FRAME(ft_INFO1)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum _frame_type {
    FOREACH_FRAME(GENERATE_ENUM)
} frame_type;

static const char* FType_STRING[] = {FOREACH_FRAME(GENERATE_STRING)};

typedef enum _frame_state {
    FOREACH_STATE(GENERATE_ENUM)
} frame_state;

static const char* FState_STRING[] = {FOREACH_STATE(GENERATE_STRING)};

#undef FOREACH_FRAME
#undef FOREACH_STATE
#undef GENERATE_ENUM
#undef GENERATE_STRING

frame_state frame_handler(frame_state cur_state, frame_type* ftype, uchar rcved);

// int read_incomming(int fd, uchar* msg_type);
// int read_info(int fd, int id);

#endif  // _SU_FRAMES_H