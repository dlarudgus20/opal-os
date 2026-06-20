#include <libuc.h>

enum {
    FBCON_IOCTL_COLOR = 0,
    FBCON_IOCTL_GET_CURSOR = 1,
    FBCON_IOCTL_GOTOXY = 2,
    FBCON_IOCTL_SCROLL_UP = 6,
};

static int g_failures;

static size_t cstr_len(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

static void raw_print(const char *str) {
    write(FD_STDOUT, str, cstr_len(str));
}

static void fbcon_newline(void) {
    long pos = ioctl(FD_STDOUT, FBCON_IOCTL_GET_CURSOR, 0);
    if (pos < 0) {
        return;
    }

    int y = (pos >> 16) & 0xffff;
    if (ioctl(FD_STDOUT, FBCON_IOCTL_GOTOXY, (unsigned long)(y + 1) << 16) < 0) {
        ioctl(FD_STDOUT, FBCON_IOCTL_SCROLL_UP, 0);
        ioctl(FD_STDOUT, FBCON_IOCTL_GOTOXY, (unsigned long)y << 16);
    }
}

static void print_long_value(long value) {
    char buf[32];
    size_t pos = sizeof(buf);
    unsigned long magnitude;

    if (value < 0) {
        raw_print("-");
        magnitude = (unsigned long)(-(value + 1)) + 1;
    } else {
        magnitude = (unsigned long)value;
    }

    do {
        buf[--pos] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);

    write(FD_STDOUT, &buf[pos], sizeof(buf) - pos);
}

static void print_result(const char *status, const char *name, long value) {
    raw_print("[");
    raw_print(status);
    raw_print("] ");
    raw_print(name);
    raw_print(" -> ");
    print_long_value(value);
    fbcon_newline();
}

static void check_int(const char *name, int value, int min_ok) {
    if (value >= min_ok) {
        print_result("ok", name, value);
        return;
    }

    print_result("fail", name, value);
    g_failures++;
}

static void check_long(const char *name, long value, long min_ok) {
    if (value >= min_ok) {
        print_result("ok", name, value);
        return;
    }

    print_result("fail", name, value);
    g_failures++;
}

static int close_checked(const char *name, int fd) {
    int ret = close(fd);
    check_int(name, ret, 0);
    return ret;
}

static void test_fbcon(void) {
    const char msg[] = "opsh syscall smoke test";
    fbcon_newline();
    ssize_t n = write(FD_STDOUT, msg, sizeof(msg) - 1);
    fbcon_newline();
    check_long("write(stdout)", n, (long)sizeof(msg) - 1);
    check_long("ioctl(fbcon color reset)", ioctl(FD_STDOUT, FBCON_IOCTL_COLOR, 0xffff), 0);
    check_long("ioctl(fbcon get cursor)", ioctl(FD_STDOUT, FBCON_IOCTL_GET_CURSOR, 0), 0);

    int dupfd = dup(FD_STDOUT, FD_INVALID);
    check_int("dup(stdout, auto)", dupfd, 0);
    if (dupfd >= 0) {
        const char dupmsg[] = " via dup";
        n = write(dupfd, dupmsg, sizeof(dupmsg) - 1);
        fbcon_newline();
        check_long("write(dup stdout)", n, (long)sizeof(dupmsg) - 1);
        close_checked("close(dup stdout)", dupfd);
    }
}

static void test_hid(void) {
    int hid = open(FD_STDIN, "/dev/hid", OPEN_READ);
    check_int("open(/dev/hid, stdin)", hid, FD_STDIN);
    if (hid >= 0) {
        close_checked("close(stdin)", FD_STDIN);
    }
}

static void test_file_read(void) {
    int fd = open(FD_INVALID, "/opsh.elf", OPEN_READ);
    check_int("open(/opsh.elf)", fd, 0);
    if (fd < 0) {
        return;
    }

    unsigned char buffer[16];
    ssize_t n = read(fd, buffer, sizeof(buffer));
    check_long("read(/opsh.elf)", n, 1);
    close_checked("close(/opsh.elf)", fd);
}

int main(void) {
    if (mount("devfs", 0, "/dev") < 0) {
        return 1;
    }

    int out = open(FD_STDOUT, "/dev/fbcon", OPEN_WRITE | OPEN_APPEND);
    if (out < 0) {
        return 1;
    }

    check_int("mount(devfs, /dev)", 0, 0);
    check_int("open(/dev/fbcon, stdout)", out, FD_STDOUT);
    test_fbcon();
    test_hid();
    test_file_read();

    fbcon_newline();
    raw_print("opsh: ");
    print_long_value(g_failures);
    raw_print(" failure(s)");
    fbcon_newline();
    return g_failures == 0 ? 0 : 1;
}
