#ifndef KC_KERRNO_H
#define KC_KERRNO_H

enum kerrno {
    KE_OK = 0,
    KERANGE,
    KEINVAL,
};

typedef enum kerrno kerrno_t;

#endif
