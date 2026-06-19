#include <stddef.h>
#include <libuc.h>
#include <kc/ctype.h>

enum {
    HID_KEY_BACKSPACE = 30,
};

struct hid_char {
    bool raw;
    char ch;
    unsigned char keycode;
};

enum {
    FBCON_IOCTL_COLOR = 0,
    FBCON_IOCTL_GET_CURSOR = 1,
    FBCON_IOCTL_GOTOXY = 2,
    FBCON_IOCTL_SET_CURSOR_VISIBLE = 3,
    FBCON_IOCTL_PUT_AT = 4,
    FBCON_IOCTL_ERASE_LINE = 5,
    FBCON_IOCTL_SCROLL_UP = 6,
};

#define TTY_LINE_MAX 256

static int read_byte(int fd) {
    unsigned char ch;
    return read(fd, &ch, 1) == 1 ? ch : -1;
}

static int write_all(int fd, const void *buffer, size_t size) {
    const char *ptr = buffer;
    size_t written = 0;
    while (written < size) {
        ssize_t n = write(fd, ptr + written, size - written);
        if (n <= 0) {
            return -1;
        }
        written += (size_t)n;
    }
    return 0;
}

static int write_byte(int fd, int ch) {
    unsigned char uch = (unsigned char)ch;
    return write_all(fd, &uch, 1);
}

static char read_escape(int fd, int *arg, size_t *arglen) {
    int ch = read_byte(fd);
    if (ch != '[') {
        return 0;
    }

    size_t maxlen = *arglen;
    size_t argi = 0;
    size_t value = 0;
    bool empty = true;
    while ((ch = read_byte(fd)) >= 0) {
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
    ioctl(FD_STDOUT, FBCON_IOCTL_COLOR, 0xffff);
}

static void color_fg(int color) {
    ioctl(FD_STDOUT, FBCON_IOCTL_COLOR, 0xff00 | color);
}

static void color_bg(int color) {
    ioctl(FD_STDOUT, FBCON_IOCTL_COLOR, (color << 8) | 0xff);
}

static long fbcon_get_cursor(void) {
    return ioctl(FD_STDOUT, FBCON_IOCTL_GET_CURSOR, 0);
}

static long fbcon_gotoxy(int x, int y) {
    return ioctl(FD_STDOUT, FBCON_IOCTL_GOTOXY, (unsigned long)(x | (y << 16)));
}

static void fbcon_put_at(int x, int y, int ch) {
    ioctl(FD_STDOUT, FBCON_IOCTL_PUT_AT, (unsigned long)(x | (y << 8) | (ch << 16)));
}

static void fbcon_scroll_up(void) {
    ioctl(FD_STDOUT, FBCON_IOCTL_SCROLL_UP, 0);
}

static void tty_newline(void) {
    long pos = fbcon_get_cursor();
    if (pos < 0) {
        return;
    }

    int y = (pos >> 16) & 0xffff;
    if (fbcon_gotoxy(0, y + 1) < 0) {
        fbcon_scroll_up();
        fbcon_gotoxy(0, y);
    }
}

static void tty_putchar(int ch) {
    if (ch == '\n') {
        tty_newline();
        return;
    }

    long pos = fbcon_get_cursor();
    if (pos < 0) {
        return;
    }

    int x = pos & 0xffff;
    int y = (pos >> 16) & 0xffff;
    fbcon_put_at(x, y, ch);
    if (fbcon_gotoxy(x + 1, y) < 0) {
        tty_newline();
    }
}

static void tty_backspace(void) {
    long pos = fbcon_get_cursor();
    if (pos < 0) {
        return;
    }

    int x = pos & 0xffff;
    int y = (pos >> 16) & 0xffff;
    if (x == 0) {
        return;
    }

    fbcon_gotoxy(x - 1, y);
    fbcon_put_at(x - 1, y, ' ');
    fbcon_gotoxy(x - 1, y);
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
    while ((ch = read_byte(fd)) >= 0) {
        if (ch == '\x1b') {
            escape(fd);
        } else {
            tty_putchar(ch);
        }
    }
}

static void read_tty(int fd) {
    char line[TTY_LINE_MAX];
    size_t line_len = 0;
    struct hid_char input;
    while (read(FD_STDIN, &input, sizeof(input)) == sizeof(input)) {
        if (input.raw) {
            if (input.keycode == HID_KEY_BACKSPACE && line_len > 0) {
                line_len--;
                tty_backspace();
            }
            continue;
        }

        int ch = (unsigned char)input.ch;
        if (ch == '\r' || ch == '\n') {
            tty_putchar('\n');
            write_all(fd, line, line_len);
            write_byte(fd, '\n');
            line_len = 0;
            continue;
        }

        if (isprint(ch)) {
            if (line_len >= sizeof(line)) {
                continue;
            }
            line[line_len++] = (char)ch;
            tty_putchar(ch);
            continue;
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
