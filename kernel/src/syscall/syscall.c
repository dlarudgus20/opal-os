#include <kc/string.h>

#include <opal/tty.h>
#include <opal/mm/kmalloc.h>
#include <opal/fs/vfs.h>
#include <opal/fs/fs_type.h>
#include <opal/fs/globals.h>
#include <opal/fs/pipefs.h>
#include <opal/task/task.h>
#include <opal/task/process.h>
#include <opal/syscall/syscall.h>

#define SYSCALL_PATH_MAX 4095
#define SYSCALL_FSNAME_MAX 255

static kerrno_t copy_user_string(uintptr_t user, size_t maxlen, char **out, size_t *outlen) {
    const char *uptr = (const char *)user;
    if (!uptr) {
        return OPAL_EINVAL;
    }

    size_t len = strnlen_s(uptr, maxlen + 1);
    if (len > maxlen) {
        return OPAL_EINVAL;
    }

    char *kptr = kzalloc(len + 1);
    if (!kptr) {
        return OPAL_ENOMEM;
    }

    memcpy(kptr, uptr, len);
    *out = kptr;
    *outlen = len;
    return OPAL_OK;
}

static intptr_t syscall_open(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4) {
    if (arg3 & ~OPEN_MASK_ALL) {
        return OPAL_EINVAL;
    }
    if (arg4 & ~INODE_MASK_ALL) {
        return OPAL_EINVAL;
    }

    char *kpath;
    size_t kpath_len;
    kerrno_t err = copy_user_string(arg2, SYSCALL_PATH_MAX, &kpath, &kpath_len);
    if (!kerrno_ok(err)) {
        return err;
    }

    struct file *file;
    fs_ssize_t result =
        vfs_create_path(NULL, kpath, (enum inode_flags)arg4, (enum open_mode)arg3, &file);
    kfree(kpath, kpath_len + 1);
    if (!kerrno_ok(result)) {
        return result;
    }

    fd_t fd = process_open_file(process_current(), arg1, file);
    if (fd < 0) {
        goto err_file;
    }

    result = (fs_ssize_t)fd;

err_file:
    file_release(file);
    return result;
}

static intptr_t syscall_close(uintptr_t arg1) {
    if (arg1 > FD_MAX) {
        return OPAL_EINVAL;
    }
    bool ok = process_close_file(process_current(), (fd_t)arg1);
    return ok ? 0 : -1;
}

static intptr_t syscall_dup(uintptr_t arg1, uintptr_t arg2) {
    fd_t fd = process_dup_fd(process_current(), arg1, arg2);
    return fd;
}

static intptr_t syscall_stat(uintptr_t arg1) {
    if (arg1 > FD_MAX) {
        return OPAL_EINVAL;
    }

    struct file *file = process_get_file(process_current(), (fd_t)arg1);
    if (!file) {
        return OPAL_ENOENT;
    }

    fs_uint_or_err stat = file_stat(file);

    file_release(file);
    return stat;
}

static intptr_t syscall_read(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    if (arg1 > FD_MAX) {
        return OPAL_EINVAL;
    }
    if (arg3 > FS_SSIZE_MAX || (arg2 == 0 && arg3 != 0)) {
        return OPAL_EINVAL;
    }

    struct file *file = process_get_file(process_current(), (fd_t)arg1);
    if (!file) {
        return OPAL_ENOENT;
    }

    fs_ssize_t ret = file_read(file, (void *)arg2, (fs_size_t)arg3);

    file_release(file);
    return ret;
}

static intptr_t syscall_write(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    if (arg1 > FD_MAX) {
        return OPAL_EINVAL;
    }
    if (arg3 > FS_SSIZE_MAX || (arg2 == 0 && arg3 != 0)) {
        return OPAL_EINVAL;
    }

    struct file *file = process_get_file(process_current(), (fd_t)arg1);
    if (!file) {
        return OPAL_ENOENT;
    }

    fs_ssize_t ret = file_write(file, (const void *)arg2, (fs_size_t)arg3);

    file_release(file);
    return ret;
}

static intptr_t syscall_ioctl(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    if (arg1 > FD_MAX) {
        return OPAL_EINVAL;
    }

    struct file *file = process_get_file(process_current(), (fd_t)arg1);
    if (!file) {
        return OPAL_ENOENT;
    }

    fs_ssize_t ret = file_ioctl(file, arg2, arg3);

    file_release(file);
    return ret;
}

static intptr_t syscall_mount(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    char *path;
    size_t path_len;
    kerrno_t err = copy_user_string(arg3, SYSCALL_PATH_MAX, &path, &path_len);
    if (!kerrno_ok(err)) {
        return err;
    }

    char *name;
    size_t name_len;
    err = copy_user_string(arg1, SYSCALL_FSNAME_MAX, &name, &name_len);
    if (!kerrno_ok(err)) {
        goto err_path;
    }

    struct fs_type *fs = vfs_fstype_get(name);
    kfree(name, name_len + 1);
    if (!fs) {
        err = OPAL_ENOENT;
        goto err_path;
    }

    struct block_device *bdev = NULL;
    if (arg2 != 0) {
        err = OPAL_ENOTSUPP;
        goto err_path;
    }

    struct superblock *sb;
    err = fs->mount(bdev, &sb);
    if (!kerrno_ok(err)) {
        goto err_path;
    }

    struct path_entry *pe;
    err = vfs_mount_path(NULL, path, sb, &pe);
    kfree(path, path_len + 1);
    if (kerrno_ok(err)) {
        path_entry_release(pe);
    }

    return err;

err_path:
    kfree(path, path_len + 1);
    return err;
}

static void syscall_pipe(struct sysret *sysret) {
    kerrno_t result = OPAL_ENOMEM;

    struct pipefs *pipe = pipefs_create();
    if (!pipe) {
        result = OPAL_ENOMEM;
        goto err;
    }

    struct file *read_file = pipefs_open_reader(pipe);
    if (!read_file) {
        goto err_pipe;
    }

    struct file *write_file = pipefs_open_writer(pipe);
    if (!write_file) {
        goto err_read_file;
    }

    struct process *proc = process_current();
    fd_t read_fd = process_open_file(proc, FD_INVALID, read_file);
    if (read_fd == FD_INVALID) {
        result = OPAL_ENOSPC;
        goto err_write_file;
    }

    fd_t write_fd = process_open_file(proc, FD_INVALID, write_file);
    if (write_fd == FD_INVALID) {
        result = OPAL_ENOSPC;
        goto err_read_fd;
    }

    sysret->ret0 = read_fd;
    sysret->ret1 = write_fd;

    file_release(write_file);
    file_release(read_file);
    inode_release(&pipe->inode);
    return;

err_read_fd:
    process_close_file(proc, read_fd);
err_write_file:
    file_release(write_file);
err_read_file:
    file_release(read_file);
err_pipe:
    inode_release(&pipe->inode);
err:
    sysret->ret0 = result;
}

static intptr_t syscall_fork(struct isr_stackframe *frame) {
    procptr_t proc;
    taskptr_t task;
    kerrno_t result = process_fork(frame, &proc, &task);
    if (!kerrno_ok(result)) {
        return result;
    }

    pid_t pid = proc.ptr->id;
    task_resume(task.ptr);
    task_release(task);
    process_release(proc);
    return pid;
}

static intptr_t syscall_exec(uintptr_t arg1) {
    if (arg1 > FD_MAX) {
        return OPAL_EINVAL;
    }

    struct process *proc = process_current();
    struct file *file = process_get_file(proc, (fd_t)arg1);
    if (!file) {
        return OPAL_ENOENT;
    }

    taskptr_t task;
    kerrno_t result = process_exec_elf_file(proc, file, &task);
    file_release(file);
    if (!kerrno_ok(result)) {
        return result;
    }

    task_resume(task.ptr);
    task_release(task);
    task_exit();
}

struct sysret syscall_dispatch(struct isr_stackframe *frame, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5) {
    struct sysret sysret = { OPAL_EUNKNOWN, 0, 0 };
    switch (arg0) {
        case SYS_TASK_EXIT:
            task_exit();
            sysret.ret0 = 0;
            break;
        case SYS_OPEN:
            sysret.ret0 = syscall_open(arg1, arg2, arg3, arg4);
            break;
        case SYS_CLOSE:
            sysret.ret0 = syscall_close(arg1);
            break;
        case SYS_DUP:
            sysret.ret0 = syscall_dup(arg1, arg2);
            break;
        case SYS_STAT:
            sysret.ret0 = syscall_stat(arg1);
            break;
        case SYS_READ:
            sysret.ret0 = syscall_read(arg1, arg2, arg3);
            break;
        case SYS_WRITE:
            sysret.ret0 = syscall_write(arg1, arg2, arg3);
            break;
        case SYS_IOCTL:
            sysret.ret0 = syscall_ioctl(arg1, arg2, arg3);
            break;
        case SYS_MOUNT:
            sysret.ret0 = syscall_mount(arg1, arg2, arg3);
            break;
        case SYS_PIPE:
            syscall_pipe(&sysret);
            break;
        case SYS_FORK:
            sysret.ret0 = syscall_fork(frame);
            break;
        case SYS_EXEC:
            sysret.ret0 = syscall_exec(arg1);
            break;
    }
    (void)arg5;
    return sysret;
}
