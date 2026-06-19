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
    ioctl(1, 0, 0xffff);
}

static void color_fg(int color) {
    ioctl(1, 0, 0xff00 | color);
}

static void color_bg(int color) {
    ioctl(1, 0, (color << 8) | 0xff);
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

static void tty(int fd) {
    int ch;
    while ((ch = readc(fd)) >= 0) {
        if (ch == '\x1b') {
            escape(fd);
        } else {
            writec(1, ch);
        }
    }
}

int main(void) {
    mount("devfs", 0, "/dev");

    if (open(0, "/dev/hid", OPEN_READ) < 0) {
        return 1;
    }
    if (open(1, "/dev/fbcon", OPEN_WRITE | OPEN_APPEND) < 0) {
        return 1;
    }
    if (dup(1, 2) < 0) {
        return 1;
    }

    while (1) {
        int io[2] = {};
        if (pipe(io) < 0) {
            break;
        }

        pid_t pid = fork();
        if (pid) {
            close(io[1]);
            tty(io[0]);
            close(io[0]);
        } else {
            close(1);
            close(2);
            dup(io[1], 1);
            dup(io[1], 2);
            close(io[0]);
            close(io[1]);

            int shfd = open(FD_INVALID, "/opsh", OPEN_READ);
            exec(shfd);
            break;
        }
    }

    return 1;
}
