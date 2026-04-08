#ifndef OPAL_FS_CPIO_H
#define OPAL_FS_CPIO_H

#include <stddef.h>

#include <opal/fs/vfs.h>

fs_status_t cpio_mount(void *cpio, size_t len, struct superblock **sb_out);

#endif
