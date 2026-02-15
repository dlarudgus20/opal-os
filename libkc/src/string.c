#define NO_BUILTIN_MACRO
#include <kc/string.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;
    size_t i;

    for (i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;
    size_t i;

    if (d == s || n == 0) {
        return dest;
    }

    if (d < s) {
        for (i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

void *memset(void *dest, int ch, size_t n) {
    unsigned char *d = (unsigned char *) dest;
    unsigned char c = (unsigned char) ch;
    size_t i;

    for (i = 0; i < n; i++) {
        d[i] = c;
    }

    return dest;
}

int memcmp(const void *lhs, const void *rhs, size_t n) {
    const unsigned char *a = (const unsigned char *) lhs;
    const unsigned char *b = (const unsigned char *) rhs;
    size_t i;

    for (i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return (int) a[i] - (int) b[i];
        }
    }

    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;

    while (s[len] != '\0') {
        len++;
    }

    return len;
}

size_t strnlen_s(const char *s, size_t strsz) {
    size_t len = 0;

    while (len < strsz && s[len] != '\0') {
        len++;
    }

    return len;
}

size_t strspn(const char *s, const char *accept) {
    size_t i;
    size_t j;
    int matched;

    for (i = 0; s[i] != '\0'; i++) {
        matched = 0;
        for (j = 0; accept[j] != '\0'; j++) {
            if (s[i] == accept[j]) {
                matched = 1;
                break;
            }
        }
        if (!matched) {
            return i;
        }
    }

    return i;
}

char *strchr(const char *s, int ch) {
    unsigned char c = (unsigned char) ch;

    while (*s != '\0') {
        if ((unsigned char) *s == c) {
            return (char *) s;
        }
        s++;
    }

    if (c == '\0') {
        return (char *) s;
    }

    return 0;
}

char *strnchr(const char *s, int ch, size_t n) {
    unsigned char c = (unsigned char) ch;
    size_t i;

    for (i = 0; i < n && s[i] != '\0'; i++) {
        if ((unsigned char) s[i] == c) {
            return (char *) (s + i);
        }
    }

    if (c == '\0' && i < n) {
        return (char *) (s + i);
    }

    return 0;
}

int strcmp(const char *lhs, const char *rhs) {
    while (*lhs != '\0' && *lhs == *rhs) {
        lhs++;
        rhs++;
    }

    return (int) (unsigned char) *lhs - (int) (unsigned char) *rhs;
}

int strncmp(const char *lhs, const char *rhs, size_t n) {
    size_t i;

    for (i = 0; i < n; i++) {
        if (lhs[i] != rhs[i]) {
            return (int) (unsigned char) lhs[i] - (int) (unsigned char) rhs[i];
        }
        if (lhs[i] == '\0') {
            return 0;
        }
    }

    return 0;
}

char *strcpy(char *restrict dest, const char *restrict src) {
    size_t i = 0;

    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';

    return dest;
}

char *strncpy(char *restrict dest, const char *restrict src, size_t n) {
    size_t i = 0;

    while (i < n && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }

    while (i < n) {
        dest[i] = '\0';
        i++;
    }

    return dest;
}
