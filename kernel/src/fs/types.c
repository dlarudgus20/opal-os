#include <opal/fs/types.h>

const char *fs_status_str(fs_status_t status) {
    if (status >= 0) {
        return "FS_OK";
    }

    switch (status) {
        case FS_ERR_IO: return "FS_ERR_IO";
        case FS_ERR_NOMEM: return "FS_ERR_NOMEM";
        case FS_ERR_BUSY: return "FS_ERR_BUSY";
        case FS_ERR_NOENT: return "FS_ERR_NOENT";
        case FS_ERR_EXIST: return "FS_ERR_EXIST";
        case FS_ERR_RANGE: return "FS_ERR_RANGE";
        case FS_ERR_INVAL: return "FS_ERR_INVAL";
        case FS_ERR_ISDIR: return "FS_ERR_ISDIR";
        case FS_ERR_NOTDIR: return "FS_ERR_NOTDIR";
        case FS_ERR_TOOBIG: return "FS_ERR_TOOBIG";
        case FS_ERR_NOTSUPP: return "FS_ERR_NOTSUPP";
        case FS_ERR_NOSPC: return "FS_ERR_NOSPC";
        default: return "FS_ERR_UNKNOWN";
    }
}
