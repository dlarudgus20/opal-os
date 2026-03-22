#include <kc/string.h>

#include <opal/tty.h>
#include <opal/shell/utils.h>

static bool is_ascii_printable(unsigned char c) {
    return c >= 0x20 && c <= 0x7e;
}

void shell_hexdump(const unsigned char *ptr, size_t len) {
    char line[16];
    size_t dumped = 0;

    while (dumped < len) {
        size_t chunk_len = len - dumped;
        if (chunk_len > 256) {
            chunk_len = 256;
        }

        size_t chunk_end = dumped + chunk_len;
        for (size_t off = dumped; off < chunk_end; off += 16) {
            tty0_printf("%08zx: ", off);

            for (size_t i = 0; i < 16; i++) {
                if (off + i < chunk_end) {
                    tty0_printf("%02x ", ptr[off + i]);
                } else {
                    tty0_puts("   ");
                }
            }

            tty0_puts(" |");
            for (size_t i = 0; i < 16 && off + i < chunk_end; i++) {
                unsigned char c = ptr[off + i];
                tty0_printf("%c", is_ascii_printable(c) ? c : '.');
            }
            tty0_puts("|\n");
        }

        dumped += chunk_len;
        if (dumped >= len) {
            break;
        }

        tty0_puts("quit or enter: ");
        tty0_getline(line, sizeof(line));
        if (strcmp(line, "q") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
    }
}
