#include <opalsys/opalsys.h>
#include <libuc.h>

static int g_failures;

static void print_result(const char *status, const char *name, long value) {
    printf("[%s] %s -> %d\n", status, name, (int)value);
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

static void test_stdio(void) {
    const char msg[] = "utest syscall smoke test";
    ssize_t n = write(FD_STDOUT, msg, sizeof(msg) - 1);
    putchar('\n');
    check_long("write(stdout)", n, (long)sizeof(msg) - 1);

    int dupfd = dup(FD_STDOUT, FD_INVALID);
    check_int("dup(stdout, auto)", dupfd, 0);
    if (dupfd >= 0) {
        const char dupmsg[] = " via dup";
        n = write(dupfd, dupmsg, sizeof(dupmsg) - 1);
        putchar('\n');
        check_long("write(dup stdout)", n, (long)sizeof(dupmsg) - 1);
        close_checked("close(dup stdout)", dupfd);
    }
}

static void test_file_read(void) {
    int fd = open(FD_INVALID, "/utest", OPEN_READ, 0);
    check_int("open(/utest)", fd, 0);
    if (fd < 0) {
        return;
    }

    unsigned char buffer[16];
    ssize_t n = read(fd, buffer, sizeof(buffer));
    check_long("read(/utest)", n, 1);
    close_checked("close(/utest)", fd);
}

static void test_pipe(void) {
    int fds[2];
    int ret = pipe(fds);
    check_int("pipe()", ret, 0);
    if (ret < 0) {
        return;
    }

    const char input[] = "pipe";
    char output[sizeof(input)];
    ssize_t n = write(fds[1], input, sizeof(input));
    check_long("write(pipe)", n, (long)sizeof(input));
    if (n != (ssize_t)sizeof(input)) {
        close_checked("close(pipe read)", fds[0]);
        close_checked("close(pipe write)", fds[1]);
        return;
    }

    n = read(fds[0], output, sizeof(output));
    check_long("read(pipe)", n, (long)sizeof(output));
    if (n != (ssize_t)sizeof(output)) {
        close_checked("close(pipe read)", fds[0]);
        close_checked("close(pipe write)", fds[1]);
        return;
    }

    int ok = 1;
    for (size_t i = 0; i < sizeof(input); i++) {
        if (input[i] != output[i]) {
            ok = 0;
            break;
        }
    }
    check_int("pipe data", ok, 1);

    close_checked("close(pipe read)", fds[0]);
    close_checked("close(pipe write)", fds[1]);
}

int main(void) {
    test_stdio();
    test_file_read();
    test_pipe();

    printf("utest: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
