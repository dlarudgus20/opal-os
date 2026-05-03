#include <stdint.h>

#include "syscall.h"

int main(void) {
    int fd = open("/test.txt");
    if (fd < 0) {
        puts("opsh: open failed");
        return 1;
    }

    int ec = 0;
    for (size_t pos = 0;; pos++) {
        int ch = readc(fd, pos);
        if (ch < 0) {
            break;
        }
        if (putchar(ch) < 0) {
            puts("\nopsh: i/o failed");
            ec = 1;
            goto end;
        }
    }
    putchar('\n');

end:
    if (close(fd) < 0) {
        puts("opsh: close failed");
        ec = 1;
    }
    return ec;
}
