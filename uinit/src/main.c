#include <stddef.h>
#include <libuc.h>
#include <kc/ctype.h>

static char read_escape(int fd, int *arg, size_t *arglen) {
    int ch = readc(fd);
    if (ch != '[') {
        return 0;
    }

    size_t maxlen = *arglen;
    size_t argi = 0;
    size_t value = 0;
    bool empty = true;
    while ((ch = readc(fd)) >= 0) {
        if (isdigit(ch)) {
            value = value * 10 + (ch - '0');
            empty = false;
        } else if (ch == ';') {
            if (argi < maxlen) {
                arg[argi++] = value;
            }
            value = 0;
            empty = true;
        } else if (isalpha(ch)) {
            if (!empty && argi < maxlen) {
                arg[argi++] = value;
            }
            *arglen = argi;
            return ch;
        } else {
            break;
        }
    }
    return 0;
}

static void color_reset(void) {
    ioctl(FD_STDOUT, 0, 0xffff);
}

static void color_fg(int color) {
    ioctl(FD_STDOUT, 0, 0xff00 | color);
}

static void color_bg(int color) {
    ioctl(FD_STDOUT, 0, (color << 8) | 0xff);
}

static void esc_color(int *arg, size_t arglen) {
    if (arglen == 0) {
        color_reset();
    }
    for (size_t i = 0; i < arglen; i++) {
        if (arg[i] == 0) {
            color_reset();
        } else if (30 <= arg[i] && arg[i] <= 37) {
            color_fg(arg[i] - 30);
        } else if (40 <= arg[i] && arg[i] <= 47) {
            color_bg(arg[i] - 40);
        } else if (90 <= arg[i] && arg[i] <= 97) {
            color_fg(arg[i] - 90 + 8);
        } else if (100 <= arg[i] && arg[i] <= 107) {
            color_bg(arg[i] - 100 + 8);
        }
    }
}

static void escape(int fd) {
    int arg[6];
    size_t arglen = sizeof(arg) / sizeof(arg[0]);
    char op = read_escape(fd, arg, &arglen);
    switch (op) {
        case 'm':
            esc_color(arg, arglen);
            break;
    }
}

static void write_tty(int fd) {
    int ch;
    while ((ch = readc(fd)) >= 0) {
        if (ch == '\x1b') {
            escape(fd);
        } else {
            writec(FD_STDOUT, ch);
        }
    }
}

static void read_tty(int fd) {
    int ch;
    while ((ch = readc(FD_STDIN)) >= 0) {
        writec(fd, ch);
        if (isprint(ch)) {
            writec(FD_STDOUT, ch);
        }
    }
}

int main(void) {
    mount("devfs", 0, "/dev");

    if (open(FD_STDIN, "/dev/hid", OPEN_READ) < 0) {
        return 1;
    }
    if (open(FD_STDOUT, "/dev/fbcon", OPEN_WRITE | OPEN_APPEND) < 0) {
        return 1;
    }
    if (dup(FD_STDOUT, FD_STDERR) < 0) {
        return 1;
    }

    while (1) {
        int out_pipe[2] = {};
        int in_pipe[2] = {};
        if (pipe(out_pipe) < 0) {
            break;
        }
        if (pipe(in_pipe) < 0) {
            close(out_pipe[0]);
            close(out_pipe[1]);
            break;
        }

        pid_t pid_reader;
        pid_t pid_shell;
        if ((pid_reader = fork()) != 0) {
            close(in_pipe[0]);
            close(in_pipe[1]);
            close(out_pipe[1]);
            write_tty(out_pipe[0]);
            close(out_pipe[0]);
        } else if ((pid_shell = fork()) != 0) {
            close(out_pipe[0]);
            close(out_pipe[1]);
            close(in_pipe[0]);
            read_tty(in_pipe[1]);
            close(in_pipe[1]);
        } else {
            close(FD_STDIN);
            close(FD_STDOUT);
            close(FD_STDERR);
            dup(in_pipe[0], FD_STDIN);
            dup(out_pipe[1], FD_STDOUT);
            dup(out_pipe[1], FD_STDERR);
            close(out_pipe[0]);
            close(out_pipe[1]);
            close(in_pipe[0]);
            close(in_pipe[1]);

            int shfd = open(FD_INVALID, "/opsh", OPEN_READ);
            exec(shfd);
            break;
        }
    }

    return 1;
}
