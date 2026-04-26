#include <kc/string.h>

#include <opal/tty.h>
#include <opal/mm/kmalloc.h>
#include <opal/fs/vfs.h>
#include <opal/task/task.h>
#include <opal/task/process.h>
#include <opal/syscall/syscall.h>

#define SYSCALL_PATH_MAX 4095

static intptr_t syscall_tty0_putc(uintptr_t arg1) {
    char ch = (char)(unsigned char)arg1;
    tty0_puts_len(&ch, 1);
    return 0;
}

static intptr_t syscall_tty0_getc() {
    return (unsigned char)tty0_getchar();
}

static intptr_t syscall_open(uintptr_t arg1) {
    const char *upath = (const char *)arg1;
    if (!upath) {
        return FS_ERR_INVAL;
    }

    size_t len = strnlen_s(upath, SYSCALL_PATH_MAX + 1);
    if (len > SYSCALL_PATH_MAX) {
        return FS_ERR_INVAL;
    }

    char *kpath = kzalloc(len + 1);
    if (!kpath) {
        return FS_ERR_NOMEM;
    }
    memcpy(kpath, upath, len);

    struct file *file;
    fs_ssize_t result = vfs_open_path(NULL, kpath, &file);
    kfree(kpath, len + 1);
    if (result != FS_OK) {
        return result;
    }

    fd_t fd = process_open_file(process_current(), file);
    if (fd < 0) {
        goto err_file;
    }

    result = (fs_ssize_t)fd;

err_file:
    file_release(file);
    return result;
}

static intptr_t syscall_close(uintptr_t arg1) {
    if (arg1 >= FD_MAX) {
        return FS_ERR_INVAL;
    }
    bool ok = process_close_file(process_current(), (fd_t)arg1);
    return ok ? 0 : -1;
}

static intptr_t syscall_readc(uintptr_t arg1, uintptr_t arg2) {
    if (arg1 >= FD_MAX) {
        goto err;
    }

    struct file *file = process_get_file(process_current(), (fd_t)arg1);
    if (!file) {
        goto err;
    }

    char ch = '\0';
    fs_ssize_t n = file->ops->read(file, (fs_size_t)arg2, &ch, 1);
    if (n != 1) {
        goto err_file;
    }

    file_release(file);
    return (unsigned char)ch;

err_file:
    file_release(file);
err:
    return -1;
}

struct sysret syscall_dispatch(uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5) {
    struct sysret sysret = { -1, 0, 0 };
    switch (arg0) {
        case SYS_TASK_EXIT:
            task_exit();
            sysret.ret1 = 0;
            break;
        case SYS_TTY0_PUTC:
            sysret.ret1 = syscall_tty0_putc(arg1);
            break;
        case SYS_TTY0_GETC:
            sysret.ret1 = syscall_tty0_getc();
            break;
        case SYS_OPEN:
            sysret.ret1 = syscall_open(arg1);
            break;
        case SYS_CLOSE:
            sysret.ret1 = syscall_close(arg1);
            break;
        case SYS_READC:
            sysret.ret1 = syscall_readc(arg1, arg2);
            break;
    }
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    return sysret;
}
