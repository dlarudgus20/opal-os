#include <stdint.h>
#include <libuc.h>

int main(void) {
    int fd = open(FD_INVALID, "/test.txt", OPEN_READ);
    if (fd < 0) {
        puts("opsh: open failed");
        return 1;
    }

    int ec = 0;
    int ch;
    while ((ch = readc(fd)) >= 0) {
        if (putchar(ch) < 0) {
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
