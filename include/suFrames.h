#ifndef _CONST_H_
#define _CONST_H_

#define F 0x5C

#define A1 0x01  //* For commands sent by the Transmitter and Answers sent by the Receiver
#define A3 0x03  //* For commands sent by the Receiver and Answers sent by the Transmitter

#define C_SET 0b000011
#define C_DISC 0b001011
#define C_UA 0b000111
#define C_RR(X) ((X << 5) | 0b000001)
#define C_REJ(X) ((X << 5) | 0b000101)

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

typedef unsigned char uChar;

#endif  // _CONST_H_