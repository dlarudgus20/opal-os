#include <stdatomic.h>

#include <kc/ctype.h>
#include <kc/string.h>
#include <kc/assert.h>
#include <kc/stdlib.h>

#include <opal/test.h>
#include <opal/tty.h>
#include <opal/klog.h>
#include <opal/kargs.h>
#include <opal/shell/shell.h>
#include <opal/shell/shell_cmd.h>
#include <opal/mm/mm.h>
#include <opal/mm/pfn.h>
#include <opal/fb/fb.h>
#include <opal/task/task.h>
#include <opal/platform/mm/pagetable.h>
#include <opal/platform/boot/bootinfo.h>
#include <opal/platform/shell/shell_cmd.h>

#define UNAME_MSG "opal-os ("OPAL_PLATFORM" "OPAL_CONFIG")"
#define SHELL_MAX_ARGV 16

typedef int (*shell_handler_raw_t)(char *args);
typedef int (*shell_handler_argv_t)(int argc, char **argv);

struct shell_command {
    const char *name;
    const char *help;
    bool argv;
    union {
        shell_handler_raw_t raw_handler;
        shell_handler_argv_t argv_handler;
    };
};

enum shell_parse_result : uint8_t {
    SHELL_PARSE_OK = 0,
    SHELL_PARSE_TOO_MANY_ARGS,
    SHELL_PARSE_UNTERMINATED_QUOTE,
    SHELL_PARSE_TRAILING_ESCAPE,
};

static tid_t g_task_id = TID_INVALID;
static bool g_exit = false;

static char *skip_spaces(char *p) {
    while (isspace(*p)) {
        p++;
    }
    return p;
}

static enum shell_parse_result parse_argv(char *line, char **argv, int *argc_out) {
    int argc = 0;
    char *src = line;

    while (1) {
        while (isspace(*src)) {
            src++;
        }

        if (*src == '\0') {
            *argc_out = argc;
            return SHELL_PARSE_OK;
        }

        if (argc >= SHELL_MAX_ARGV) {
            return SHELL_PARSE_TOO_MANY_ARGS;
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
                        return SHELL_PARSE_TRAILING_ESCAPE;
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
                    return SHELL_PARSE_TRAILING_ESCAPE;
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
            return SHELL_PARSE_UNTERMINATED_QUOTE;
        }
        *dst = '\0';
    }
}

static int cmd_help(int argc, char **argv);
static int cmd_uname(int argc, char **argv);
static int cmd_echo(char *args);
static int cmd_exit(int argc, char **argv);
static int cmd_halt(int argc, char **argv);
static int cmd_klog(char *args);
static int cmd_kmsg(int argc, char **argv);
static int cmd_mmap(int argc, char **argv);
static int cmd_ptable(int argc, char **argv);
static int cmd_pfns(int argc, char **argv);
static int cmd_kargs(int argc, char **argv);
static int cmd_bootmodules(int argc, char **argv);
static int cmd_fbinfo(int argc, char **argv);
static int cmd_allocinfo(int argc, char **argv);
#ifdef OPAL_UNIT_TEST
static int cmd_unit_test_run(int argc, char **argv);
static int cmd_unit_test_run_heavy(int argc, char **argv);
#endif

static const struct shell_command g_commands[] = {
#define CMD_RAW(name, help, fn) { (name), (help), false, { .raw_handler = (fn) } }
#define CMD_ARGV(name, help, fn) { (name), (help), true, { .argv_handler = (fn) } }
    CMD_ARGV("help",        "show this message",                    cmd_help),
#ifdef OPAL_UNIT_TEST
    CMD_ARGV("unit",        "run unit tests",                       cmd_unit_test_run),
    CMD_ARGV("uheavy",      "run heavy unit tests",                 cmd_unit_test_run_heavy),
#endif
    CMD_ARGV("uname",       "show kernel name",                     cmd_uname),
    CMD_RAW("echo",         "print echo",                           cmd_echo),
    CMD_ARGV("exit",        "logout",                               cmd_exit),
    CMD_ARGV("halt",        "halt system",                          cmd_halt),
    CMD_RAW("klog",         "klog (level) [message]",               cmd_klog),
    CMD_ARGV("kmsg",        "read klogs",                           cmd_kmsg),
    CMD_ARGV("mmap",        "log memory map",                       cmd_mmap),
    CMD_ARGV("ptable",      "show pagetable",                       cmd_ptable),
    CMD_ARGV("pfns",        "show pfn list",                        cmd_pfns),
    CMD_ARGV("kargs",       "show kernel boot argument",            cmd_kargs),
    CMD_ARGV("bootmodules", "show boot module list",                cmd_bootmodules),
    CMD_ARGV("fbinfo",      "show framebuffer info",                cmd_fbinfo),
    CMD_ARGV("allocinfo",   "show buddy allocator info",            cmd_allocinfo),
    CMD_ARGV("irfdump",     "hexdump initramfs",                    shell_cmd_irfdump),
    CMD_ARGV("priotest",    "run priority scheduler smoke test",    shell_cmd_priotest),
    CMD_ARGV("fputest",     "run floating-point quick test",        shell_cmd_fputest),
    CMD_ARGV("floattest",   "run floating-point consistency test",  shell_cmd_floattest),
    CMD_ARGV("lsblk",       "list block devices",                   shell_cmd_lsblk),
    CMD_ARGV("lspart",      "list partitions: lspart [disk]",       shell_cmd_lspart),
    CMD_ARGV("mkpart",      "create partition: mkpart ...",         shell_cmd_mkpart),
    CMD_ARGV("rmpart",      "remove partition: rmpart ...",         shell_cmd_rmpart),
    CMD_ARGV("diskreset",   "reset partition table: diskreset N",   shell_cmd_diskreset),
    CMD_ARGV("diskrescan",  "rescan partitions: diskrescan N",      shell_cmd_diskrescan),
    CMD_ARGV("readsec",     "read sectors: readsec D L C",          shell_cmd_rwsec),
    CMD_ARGV("writesec",    "write sectors: writesec D L C V",      shell_cmd_rwsec),
    CMD_ARGV("testrwsec",   "test rw sectors: testrwsec",           shell_cmd_testrwsec),
};

static const struct shell_command *find_command(const char *name, size_t len) {
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
    const struct shell_command *command = find_command(input, cmd_len);
    if (!command) {
        tty0_puts("unknown command: ");
        tty0_puts_len(input, cmd_len);
        tty0_puts("\n");
        return 1;
    }

    if (!command->argv) {
        return command->raw_handler(skip_spaces(p));
    }

    char *argv[SHELL_MAX_ARGV];
    int argc = 0;
    enum shell_parse_result parse_result = parse_argv(input, argv, &argc);
    if (parse_result != SHELL_PARSE_OK) {
        if (parse_result == SHELL_PARSE_TOO_MANY_ARGS) {
            tty0_printf("shell: too many arguments (max=%u)\n", (unsigned)SHELL_MAX_ARGV);
        } else if (parse_result == SHELL_PARSE_UNTERMINATED_QUOTE) {
            tty0_puts("shell: unterminated quote\n");
        } else {
            tty0_puts("shell: trailing escape\n");
        }
        return 1;
    }

    return command->argv_handler(argc, argv);
}

static void print_banner(void) {
    tty0_puts("\n");
    tty0_puts("========================================\n");
    tty0_puts("  "UNAME_MSG"\n");
    tty0_puts("========================================\n");
}

static void run_shell(uintptr_t) {
    char line[128];

    while (1) {
        print_banner();

        do {
            tty0_puts("root@opal:~$ ");
            tty0_getline(line, sizeof(line));

            g_exit = false;
            handle_command(line);
        } while (!g_exit);
    }
}

void shell_start(void) {
    taskptr_t task = task_create(run_shell, 0, TASK_PRIORITY_NORMAL);
    if (!task.ptr) {
        return;
    }

    g_task_id = task_release(task);
}

static int cmd_help(int, char **) {
    tty0_puts("commands:\n");
    for (size_t i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); i++) {
        tty0_puts("  ");
        tty0_puts(g_commands[i].name);
        tty0_puts(" - ");
        tty0_puts(g_commands[i].help);
        tty0_puts("\n");
    }
    return 0;
}

#ifdef OPAL_UNIT_TEST
static int cmd_unit_test_run(int, char **) {
    unit_test_run();
    return 0;
}

static int cmd_unit_test_run_heavy(int , char **) {
    unit_test_run_heavy();
    return 0;
}
#endif

static int cmd_uname(int, char **) {
    tty0_puts(UNAME_MSG "\n");
    return 0;
}

static int cmd_echo(char *args) {
    tty0_puts(args);
    tty0_puts("\n");
    return 0;
}

static int cmd_exit(int, char **) {
    tty0_puts("logout\n");
    g_exit = true;
    return 0;
}

static int cmd_halt(int, char **) {
    panic("system halt is not implemented");
}

static int cmd_klog(char *args) {
    uint16_t level = KLOG_INFO;
    char *msg = args;

    unsigned long parsed = 0;
    char *endptr = NULL;
    if (kstrtoul(args, 10, &endptr, &parsed) == KE_OK && parsed <= UINT16_MAX) {
        level = (uint16_t)parsed;
        msg = skip_spaces(endptr);
    }

    klogf(level, "%s", msg);
    return 0;
}

static int cmd_kmsg(int, char **) {
    klog_print_all_tty0(true);
    return 0;
}

static int cmd_mmap(int, char **) {
    mm_log_map();
    return 0;
}

static int cmd_ptable(int, char **) {
    mm_pagetable_print();
    return 0;
}

static int cmd_pfns(int, char **) {
    pfn_print_all();
    return 0;
}

static int cmd_kargs(int, char **) {
    kargs_print_log();
    tty0_puts("\n");
    return 0;
}

static int cmd_bootmodules(int, char **) {
    const struct bootinfo_module_list *modules = bootinfo_get_modules();
    tty0_printf("boot modules: %u\n", modules->len);
    for (uint32_t i = 0; i < modules->len; i++) {
        const struct bootinfo_module *module = &modules->modules[i];
        tty0_printf("  [%u] [%#018" PRIphys ", %#018" PRIphys ") %s\n",
            i, module->begin, module->end, module->name);
    }
    return 0;
}

static int cmd_fbinfo(int, char **) {
    const struct bootinfo_fb *fbinfo = bootinfo_get_fb();
    if (fbinfo) {
        tty0_printf("addr=%#010" PRIphys ", %ux%u, pitch=%u, bpp=%u\n",
            fbinfo->addr, fbinfo->width, fbinfo->height, fbinfo->pitch, fbinfo->bpp);
    } else {
        tty0_printf("there is no framebuffer.\n");
    }
    return 0;
}

static int cmd_allocinfo(int, char **) {
    mm_log_buddy();
    return 0;
}
