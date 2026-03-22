#ifndef KC_ERRNO_H
#define KC_ERRNO_H

enum errno {
    E_OK = 0,
    ERANGE,
    EINVAL,
};

typedef enum errno errno_t;

#endif
