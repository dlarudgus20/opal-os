#include <opal/tty.h>
#include <opal/fs/vfs.h>
#include <opal/task/process.h>
#include <opal/shell/shell_cmd.h>

int shell_cmd_exec(int argc, char **argv) {
    if (argc != 2) {
        tty0_puts("usage: exec [path]\n");
        return 1;
    }

    int ret = 1;

    struct file *file = NULL;
    kerrno_t result = vfs_open_path(NULL, argv[1], OPEN_READ, &file);
    if (!kerrno_ok(result)) {
        tty0_printf("exec: vfs_open_path: %s (%d)\n", kerrno_str(result), result);
        return 1;
    }

    procptr_t proc;
    taskptr_t task;
    result = process_load_elf_file(file, &proc, &task);
    if (!kerrno_ok(result)) {
        tty0_printf("exec: cannot load elf: %s (%d)\n", kerrno_str(result), result);
        goto err_file;
    }

    task_join(task.ptr, TIMEOUT_INFINITY);
    task_release(task);

    ret = 0;
    process_release(proc);
err_file:
    file_release(file);
    return ret;
}
