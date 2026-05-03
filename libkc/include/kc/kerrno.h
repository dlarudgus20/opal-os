#ifndef KC_KERRNO_H
#define KC_KERRNO_H

enum kerrno {
    OPAL_OK = 0,

    OPAL_EIO = -1,
    OPAL_ENOMEM = -2,
    OPAL_EBUSY = -3,
    OPAL_ENOENT = -4,
    OPAL_EEXIST = -5,
    OPAL_ERANGE = -6,
    OPAL_EINVAL = -7,
    OPAL_ETOOBIG = -10,
    OPAL_ENOTSUPP = -11,
    OPAL_ENOSPC = -12,

    OPAL_EISDIR = -100,
    OPAL_ENOTDIR = -101,

    OPAL_EBADIMAGE = -200,
    OPAL_ENOEXEC = -201,

    OPAL_EUNKNOWN = -1000,
};

typedef enum kerrno kerrno_t;

const char *kerrno_str(kerrno_t err);

// implemented as macro to comfort gcc analyzer
#define kerrno_ok(err) ((err) >= 0)

#endif
