#include <kc/kerrno.h>

const char *kerrno_str(kerrno_t err) {
    if (err >= 0) {
        return "OPAL_OK";
    }

    switch (err) {
        case OPAL_EIO: return "OPAL_EIO";
        case OPAL_ENOMEM: return "OPAL_ENOMEM";
        case OPAL_EBUSY: return "OPAL_EBUSY";
        case OPAL_ENOENT: return "OPAL_ENOENT";
        case OPAL_EEXIST: return "OPAL_EEXIST";
        case OPAL_ERANGE: return "OPAL_ERANGE";
        case OPAL_EINVAL: return "OPAL_EINVAL";
        case OPAL_ETOOBIG: return "OPAL_ETOOBIG";
        case OPAL_ENOTSUPP: return "OPAL_ENOTSUPP";
        case OPAL_ENOSPC: return "OPAL_ENOSPC";

        case OPAL_EISDIR: return "OPAL_EISDIR";
        case OPAL_ENOTDIR: return "OPAL_ENOTDIR";

        case OPAL_EBADIMAGE: return "OPAL_EBADIMAGE";
        case OPAL_ENOEXEC: return "OPAL_ENOEXEC";

        default: return "OPAL_EUNKNOWN";
    }
}
