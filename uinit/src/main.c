#include <libuc.h>

int main(void) {
    mount("devfs", 0, "/dev");

    int out = open(FD_STDOUT, "/dev/fbcon", OPEN_WRITE | OPEN_APPEND);
    if (out < 0) {
        return 1;
    }

    int fd = open(FD_INVALID, "/opsh", OPEN_READ);
    if (fd < 0) {
        puts("uinit: cannot open /opsh");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        puts("uinit: fork failed");
        return 1;
    }

    if (pid == 0) {
        exec(fd);
        return 1;
    }

    puts("uinit: fork succeeded");
    return 0;
}
