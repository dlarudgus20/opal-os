#include <stddef.h>
#include <stdint.h>

#include <kc/stdio.h>
#include <kc/ctype.h>
#include <kc/string.h>
#include <kc/assert.h>

#include <opal/tty.h>
#include <opal/klog.h>
#include <opal/mm/mm.h>
#include <opal/mm/map.h>
#include <opal/drivers/uart.h>

static void print_mmap(const struct mmap *mmap) {
    for (uint32_t i = 0; i < mmap->length; i++) {
        const struct mmap_entry *entry = &mmap->entries[i];
        tty0_printf("  [%d] base=0x%llx len=0x%llx type=%u\n",
            i, (unsigned long long)entry->addr, (unsigned long long)entry->len, entry->type);
    }
}

static void print_memory_map(void) {
    tty0_puts("boot memory map:\n");
    print_mmap(mm_get_boot_map());
    tty0_puts("sanitized memory map:\n");
    print_mmap(mm_get_sanitized_map());
}

static void print_banner(void) {
    tty0_puts("\n");
    tty0_puts("========================================\n");
    tty0_puts("  opal-os UART shell PoC (ssh-like)\n");
    tty0_puts("========================================\n");
}

static int handle_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        tty0_puts("commands:\n");
        tty0_puts("  help      - show this message\n");
        tty0_puts("  uname     - show kernel name\n");
        tty0_puts("  whoami    - current user\n");
        tty0_puts("  clear     - clear terminal\n");
        tty0_puts("  echo TEXT - print TEXT\n");
        tty0_puts("  exit      - logout\n");
        tty0_puts("  halt      - halt system\n");
        tty0_puts("  klog TEXT - log TEXT\n");
        tty0_puts("  kmsg      - read logs\n");
        return 1;
    }

    if (strcmp(cmd, "uname") == 0) {
        tty0_puts("opal-os pc-x64 (uart-poc)\n");
        return 1;
    }

    if (strcmp(cmd, "whoami") == 0) {
        tty0_puts("root\n");
        return 1;
    }

    if (strcmp(cmd, "clear") == 0) {
        tty0_puts("\x1b[2J\x1b[H");
        return 1;
    }

    if (strncmp(cmd, "echo", 4) == 0) {
        const char *text = cmd + 4 + strspn(cmd + 4, " ");
        tty0_puts(text);
        tty0_puts("\n");
        return 1;
    }

    if (strcmp(cmd, "exit") == 0) {
        tty0_puts("logout\n");
        return 0;
    }

    if (strcmp(cmd, "halt") == 0) {
        panic("system halt is not implemented");
    }

    if (strncmp(cmd, "klog", 4) == 0) {
        const char *text = cmd + 4 + strspn(cmd + 4, " ");
        uint16_t level = KLOG_INFO;
        if (isdigit(*text)) {
            level = *text - '0';
            text += strspn(text + 1, " ") + 1;
        }
        klogf(level, "%s", text);
        return 1;
    }

    if (strcmp(cmd, "kmsg") == 0) {
        struct klog_record_header header;
        char msg[KLOG_MAX_MSGLEN + 1];
        const char *colors[KLOG_LEVEL_COUNT] = {
            "\x1b[1;97;41m", "\x1b[1;91m", "\x1b[1;93m", "\x1b[1;96m", "\x1b[1;92m", "\x1b[90m"
        };
        while (klog_read(&header, msg, sizeof(msg))) {
            tty0_printf("%s[%u] %s\x1b[0m\n", colors[header.level], header.seq, msg);
        }
        return 1;
    }

    if (strcmp(cmd, "halt") == 0) {
        panic("system halt is not implemented");
    }

    if (cmd[0] != '\0') {
        tty0_puts("unknown command: ");
        tty0_puts(cmd);
        tty0_puts("\n");
    }

    return 1;
}

static void run_shell(void) {
    char line[128];

    while (1) {
        tty0_puts("root@opal:~$ ");
        uart_read_line(line, sizeof(line), 0);

        if (!handle_command(line)) {
            return;
        }
    }
}

static void login_loop(void) {
    char user[32];
    char pass[32];

    while (1) {
        tty0_puts("login: ");
        uart_read_line(user, sizeof(user), 0);

        tty0_puts("password: ");
        uart_read_line(pass, sizeof(pass), 1);

        if (strcmp(user, "root") == 0 && strcmp(pass, "opal") == 0) {
            tty0_puts("authentication successful\n");
            run_shell();
            return;
        }

        tty0_puts("authentication failed\n\n");
    }
}

void kmain(void) {
    uart_init();
    tty0_init();
    klog_init();
    mm_init();

    print_banner();
    print_memory_map();

    while (1) {
        login_loop();
    }
}
