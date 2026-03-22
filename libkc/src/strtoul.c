#include <limits.h>

#include <kc/stdlib.h>
#include <kc/ctype.h>

static int digit_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'z') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A' + 10;
    }
    return -1;
}

errno_t kstrtoul(const char *str, int base, char **endptr, unsigned long *result) {
    if (!str || !result || (base != 0 && (base < 2 || base > 36))) {
        if (endptr) {
            *endptr = (char *)str;
        }
        return EINVAL;
    }

    const char *s = str;
    while (isspace(*s)) {
        s++;
    }

    if (*s == '+' || *s == '-') {
        if (*s == '-') {
            if (endptr) {
                *endptr = (char *)s;
            }
            return EINVAL;
        }
        s++;
    }

    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && digit_value(s[2]) >= 0 && digit_value(s[2]) < 16) {
            base = 16;
            s += 2;
        } else if (s[0] == '0') {
            base = 8;
        } else {
            base = 10;
        }
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')
        && digit_value(s[2]) >= 0 && digit_value(s[2]) < 16) {
        s += 2;
    }

    unsigned long value = 0;
    bool has_digit = false;
    bool overflow = false;
    const unsigned long max_value = ULONG_MAX;

    while (1) {
        int digit = digit_value(*s);
        if (digit < 0 || digit >= base) {
            break;
        }

        has_digit = true;
        if (!overflow) {
            if (value > (max_value - (unsigned long)digit) / (unsigned long)base) {
                overflow = true;
                value = max_value;
            } else {
                value = value * (unsigned long)base + (unsigned long)digit;
            }
        }
        s++;
    }

    if (!has_digit) {
        if (endptr) {
            *endptr = (char *)str;
        }
        return EINVAL;
    }

    if (endptr) {
        *endptr = (char *)s;
    }

    if (overflow) {
        return ERANGE;
    }

    *result = value;
    return E_OK;
}

errno_t kstrtoul_exact(const char *str, int base, unsigned long max, unsigned long *result) {
    char *endptr;
    errno_t err = kstrtoul(str, base, &endptr, result);
    if (err == E_OK) {
        if (*endptr != '\0') {
            return EINVAL;
        }
        if (*result > max) {
            return ERANGE;
        }
    }
    return err;
}
