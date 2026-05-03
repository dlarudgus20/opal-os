#include <opal/tty.h>
#include <opal/fs/vfs.h>
#include <opal/mm/kmalloc.h>
#include <opal/task/process.h>
#include <opal/shell/shell_cmd.h>

int shell_cmd_exec(int argc, char **argv) {
    if (argc != 2) {
        tty0_puts("usage: exec [path]\n");
        return 1;
    }

    int ret = 1;

    struct file *file = NULL;
    kerrno_t result = vfs_open_path(NULL, argv[1], &file);
    if (!kerrno_ok(result)) {
        tty0_printf("exec: vfs_open_path: %s (%d)\n", kerrno_str(result), result);
        return 1;
    }

    fs_size_t len;
    result = file->ops->seek(file, 0, FS_SEEK_END, &len);
    if (!kerrno_ok(result)) {
        tty0_printf("exec: i/o error: %s (%d)\n", kerrno_str(result), result);
        goto err_file;
    }

    void *buf = kzalloc(len);
    if (!buf) {
        tty0_printf("exec: out of memory\n");
        goto err_file;
    }

    fs_ssize_t n = file->ops->read(file, 0, buf, len);
    if (n < 0) {
        tty0_printf("exec: i/o error: %s (%d)\n", kerrno_str((kerrno_t)n), (kerrno_t)n);
        goto err_alloc;
    } else if ((fs_size_t)n != len) {
        tty0_printf("exec: cannot read all of file\n");
        goto err_alloc;
    }

    procptr_t proc = process_create();
    if (!proc.ptr) {
        tty0_printf("exec: out of memory\n");
        goto err_alloc;
    }

    taskptr_t task;
    kerrno_t load_result = process_load_elf(proc.ptr, buf, len, &task);
    if (!kerrno_ok(load_result)) {
        tty0_printf("exec: cannot load elf: %d\n", load_result);
        goto err_proc;
    }

    task_join(task.ptr, TIMEOUT_INFINITY);
    task_release(task);

    ret = 0;
err_proc:
    process_release(proc);
err_alloc:
    kfree(buf, len);
err_file:
    file_release(file);
    return ret;
}
