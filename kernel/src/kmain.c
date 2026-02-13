#include <kc/string.h>
#include <opal/drivers/uart.h>

static void print_banner(void) {
    uart_write("\n");
    uart_write("========================================\n");
    uart_write("  opal-os UART shell PoC (ssh-like)\n");
    uart_write("========================================\n");
}

static int handle_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        uart_write("commands:\n");
        uart_write("  help      - show this message\n");
        uart_write("  uname     - show kernel name\n");
        uart_write("  whoami    - current user\n");
        uart_write("  clear     - clear terminal\n");
        uart_write("  echo TEXT - print TEXT\n");
        uart_write("  exit      - logout\n");
        return 1;
    }

    if (strcmp(cmd, "uname") == 0) {
        uart_write("opal-os pc-x64 (uart-poc)\n");
        return 1;
    }

    if (strcmp(cmd, "whoami") == 0) {
        uart_write("root\n");
        return 1;
    }

    if (strcmp(cmd, "clear") == 0) {
        uart_write("\x1b[2J\x1b[H");
        return 1;
    }

    if (strncmp(cmd, "echo", 4) == 0) {
        const char *text = cmd + 4 + strspn(cmd + 4, " ");
        uart_write(text);
        uart_write("\n");
        return 1;
    }

    if (strcmp(cmd, "exit") == 0) {
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

        if (strcmp(user, "root") == 0 && strcmp(pass, "opal") == 0) {
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
