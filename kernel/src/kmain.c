#include <opal/drivers/uart.h>

static int str_eq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static int str_starts_with(const char *s, const char *prefix) {
    while (*prefix != '\0') {
        if (*s++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static const char *skip_spaces(const char *s) {
    while (*s == ' ') {
        s++;
    }
    return s;
}

static void print_banner(void) {
    uart_write("\n");
    uart_write("========================================\n");
    uart_write("  opal-os UART shell PoC (ssh-like)\n");
    uart_write("========================================\n");
}

static int handle_command(const char *cmd) {
    if (str_eq(cmd, "help")) {
        uart_write("commands:\n");
        uart_write("  help      - show this message\n");
        uart_write("  uname     - show kernel name\n");
        uart_write("  whoami    - current user\n");
        uart_write("  clear     - clear terminal\n");
        uart_write("  echo TEXT - print TEXT\n");
        uart_write("  exit      - logout\n");
        return 1;
    }

    if (str_eq(cmd, "uname")) {
        uart_write("opal-os pc-x64 (uart-poc)\n");
        return 1;
    }

    if (str_eq(cmd, "whoami")) {
        uart_write("root\n");
        return 1;
    }

    if (str_eq(cmd, "clear")) {
        uart_write("\x1b[2J\x1b[H");
        return 1;
    }

    if (str_starts_with(cmd, "echo")) {
        const char *text = skip_spaces(cmd + 4);
        uart_write(text);
        uart_write("\n");
        return 1;
    }

    if (str_eq(cmd, "exit")) {
        uart_write("logout\n");
        return 0;
    }

    if (cmd[0] != '\0') {
        uart_write("unknown command: ");
        uart_write(cmd);
        uart_write("\n");
    }

    return 1;
}

static void run_shell(void) {
    char line[128];

    while (1) {
        uart_write("root@opal:~$ ");
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
        uart_write("login: ");
        uart_read_line(user, sizeof(user), 0);

        uart_write("password: ");
        uart_read_line(pass, sizeof(pass), 1);

        if (str_eq(user, "root") && str_eq(pass, "opal")) {
            uart_write("authentication successful\n");
            run_shell();
            return;
        }

        uart_write("authentication failed\n\n");
    }
}

void kmain(void) {
    uart_init();
    print_banner();

    while (1) {
        login_loop();
    }
}
