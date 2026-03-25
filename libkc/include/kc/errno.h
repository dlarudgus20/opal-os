#ifndef KC_ERRNO_H
#define KC_ERRNO_H

enum kerrno {
    KE_OK = 0,
    KERANGE,
    KEINVAL,
};

typedef enum kerrno kerrno_t;

#endif
