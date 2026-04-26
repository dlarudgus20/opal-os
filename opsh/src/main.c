#include <stdint.h>

#include "syscall.h"

int main(void) {
    puts("opsh readc test");
    int fd = open("/test.txt");
    if (fd < 0) {
        puts("open failed");
        return 1;
    }

    int c0 = readc(fd, 0);
    int c1 = readc(fd, 1);
    int ceof = readc(fd, 0x7fffffff);
    printf("readc: c0=%d c1=%d eof=%d\n", c0, c1, ceof);

    if (close(fd) < 0) {
        puts("close failed");
        return 1;
    }
    int cclosed = readc(fd, 0);
    printf("readc after close=%d\n", cclosed);

    puts("good");
    return 0;
}
