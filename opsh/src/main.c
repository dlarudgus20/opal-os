#include <stdint.h>

#include <kc/ctype.h>
#include <kc/string.h>

#include <opalsys/opalsys.h>
#include <libuc.h>

#define MAX_ARGV 16

typedef int (*handler_raw_t)(char *args);
typedef int (*handler_argv_t)(int argc, char **argv);

struct cmd {
    const char *name;
    const char *help;
    handler_raw_t raw_handler;
    handler_argv_t argv_handler;
};

static int cmd_help(int argc, char **argv);
static int cmd_echo(char *args);
static int cmd_cat(int argc, char **argv);
static int cmd_ls(int argc, char **argv);

// clang-format off
static const struct cmd g_commands[] = {
#define CMD_RAW(name, help, fn) { (name), (help), .raw_handler = (fn) }
#define CMD_ARGV(name, help, fn) { (name), (help), .argv_handler = (fn) }
    CMD_ARGV("help", "show this message", cmd_help),
    CMD_RAW("echo", "print echo", cmd_echo),
    CMD_ARGV("cat", "show file content", cmd_cat),
    CMD_ARGV("ls", "list directory entries", cmd_ls),
};
// clang-format on

enum parse_argv_result {
    PARSE_OK,
    PARSE_TOO_MANY_ARGS,
    PARSE_UNTERMINATED_QUOTE,
    PARSE_TRAILING_ESCAPE,
};

static char *skip_spaces(char *p) {
    while (isspace(*p)) {
        p++;
    }
    return p;
}

static enum parse_argv_result parse_argv(char *line, char **argv, int max_argv, int *argc_out) {
    int argc = 0;
    char *src = line;

    while (1) {
        while (isspace(*src)) {
            src++;
        }

        if (*src == '\0') {
            *argc_out = argc;
            return PARSE_OK;
        }

        if (argc >= max_argv) {
            return PARSE_TOO_MANY_ARGS;
        }

        char *dst = src;
        argv[argc++] = dst;
        char quote = '\0';

        while (*src != '\0') {
            char ch = *src++;
            if (quote != '\0') {
                if (ch == quote) {
                    quote = '\0';
                    continue;
                }
                if (ch == '\\') {
                    if (*src == '\0') {
                        return PARSE_TRAILING_ESCAPE;
                    }
                    ch = *src++;
                }
                *dst++ = ch;
                continue;
            }

            if (ch == '\'' || ch == '"') {
                quote = ch;
                continue;
            }

            if (ch == '\\') {
                if (*src == '\0') {
                    return PARSE_TRAILING_ESCAPE;
                }
                *dst++ = *src++;
                continue;
            }

            if (isspace(ch)) {
                break;
            }

            *dst++ = ch;
        }

        if (quote != '\0') {
            return PARSE_UNTERMINATED_QUOTE;
        }
        *dst = '\0';
    }
}

static const struct cmd *find_command(const char *name, size_t len) {
    for (size_t i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); i++) {
        const char *cmd_name = g_commands[i].name;
        if (strncmp(cmd_name, name, len) == 0 && cmd_name[len] == '\0') {
            return &g_commands[i];
        }
    }
    return NULL;
}

static int handle_command(char *line) {
    char *input = skip_spaces(line);
    if (*input == '\0') {
        return 0;
    }

    char *p = input;
    while (*p != '\0' && !isspace(*p)) {
        p++;
    }

    const size_t cmd_len = (size_t)(p - input);
    const struct cmd *command = find_command(input, cmd_len);
    if (!command) {
        printf("unknown command: ");
        for (size_t i = 0; i < cmd_len; i++) {
            putchar(input[i]);
        }
        putchar('\n');
        return 1;
    }

    if (command->raw_handler) {
        return command->raw_handler(skip_spaces(p));
    }

    char *argv[MAX_ARGV];
    int argc = 0;
    enum parse_argv_result parse_result = parse_argv(input, argv, MAX_ARGV, &argc);
    if (parse_result != PARSE_OK) {
        if (parse_result == PARSE_TOO_MANY_ARGS) {
            printf("opsh: too many arguments (max=%d)\n", MAX_ARGV);
        } else if (parse_result == PARSE_UNTERMINATED_QUOTE) {
            puts("opsh: unterminated quote");
        } else {
            puts("opsh: trailing escape");
        }
        return 1;
    }

    return command->argv_handler(argc, argv);
}

int main(void) {
    char buf[128];

    while (1) {
        printf("opsh: /> ");
        getline(buf, sizeof(buf));
        handle_command(buf);
    }
}

static void print_help_for(const struct cmd *cmd) {
    printf("  %s - %s\n", cmd->name, cmd->help);
}

static int cmd_help(int argc, char **argv) {
    if (argc == 2) {
        const struct cmd *cmd = find_command(argv[1], strlen(argv[1]));
        if (!cmd) {
            printf("'%s' is not a valid command.\n", argv[0]);
        } else {
            print_help_for(cmd);
        }
    } else {
        puts("commands:");
        for (size_t i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); i++) {
            print_help_for(&g_commands[i]);
        }
    }
    return 0;
}

static int cmd_echo(char *args) {
    puts(args);
    return 0;
}

static int cmd_cat(int argc, char **argv) {
    if (argc != 2) {
        puts("usage: cat [path]");
        return 1;
    }

    int fd = open(FD_INVALID, argv[1], OPEN_READ, 0);
    if (fd < 0) {
        puts("cat: open failed");
        return 1;
    }

    int ec = 0;
    char buf[128];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (write(FD_STDOUT, buf, (size_t)n) != n) {
            ec = 1;
            goto end;
        }
    }
    if (n < 0) {
        ec = 1;
        goto end;
    }
    putchar('\n');

end:
    if (close(fd) < 0) {
        puts("cat: close failed");
        ec = 1;
    }
    return ec;
}

static int cmd_ls(int argc, char **argv) {
    const char *path = "/";
    if (argc == 2) {
        path = argv[1];
    } else if (argc != 1) {
        puts("usage: ls [path]");
        return 1;
    }

    int fd = open(FD_INVALID, path, OPEN_READ, 0);
    if (fd < 0) {
        puts("ls: open failed");
        return 1;
    }

    int ec = 0;
    int flags = stat(fd);
    if (flags < 0) {
        puts("ls: stat failed");
        ec = 1;
        goto end;
    }
    if (!(flags & INODE_DIR)) {
        puts("ls: not a directory");
        ec = 1;
        goto end;
    }

    unsigned char buf[512];
    while (1) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            puts("ls: read failed");
            ec = 1;
            break;
        }
        if (n == 0) {
            break;
        }

        size_t pos = 0;
        while (pos < (size_t)n) {
            struct dirent *entry = (struct dirent *)(buf + pos);
            write(FD_STDOUT, entry->name, entry->name_len);
            if (entry->flags & INODE_DIR) {
                putchar('/');
            }
            putchar('\n');
            if (entry->next_offset == 0) {
                break;
            }
            pos += entry->next_offset;
        }
    }

end:
    if (close(fd) < 0) {
        puts("ls: close failed");
        ec = 1;
    }
    return ec;
}
