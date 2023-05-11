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

#define SET(A) \
    { F, A, C_SET, A ^ C_SET, F }

#define DISC(A) \
    { F, A, C_DISC, A ^ C_DISC, F }

#define UA(A) \
    { F, A, C_UA, A ^ C_UA, F }

#define RR(A, X) \
    { F, A, C_RR(X), A ^ C_RR(X), F }

#define REJ(A, X) \
    { F, A, C_REJ(X), A ^ C_REJ(X), F }

typedef unsigned char uchar;

int read_incomming(int fd, uchar* msg_type);

#endif  // _SU_FRAMES_H