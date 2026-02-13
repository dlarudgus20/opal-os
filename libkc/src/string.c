#include <kc/string.h>

int str_eq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

int str_starts_with(const char *s, const char *prefix) {
    while (*prefix != '\0') {
        if (*s++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

const char *skip_spaces(const char *s) {
    while (*s == ' ') {
        s++;
    }
    return s;
}
