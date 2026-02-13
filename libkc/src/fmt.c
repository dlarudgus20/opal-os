#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <kc/fmt.h>
#include <kc/ctype.h>
#include <kc/string.h>

enum length_modifier {
    LEN_NONE,
    LEN_HH,
    LEN_H,
    LEN_L,
    LEN_LL,
    LEN_J,
    LEN_Z,
    LEN_T,
    LEN_CAP_L
};

struct printf_flags {
    bool left_adj;
    bool plus;
    bool space;
    bool alt;
    bool zero_pad;
    int width;
    int precision;
};

static void write_char(struct fmt *restrict fmt, char ch) {
    if (fmt->error) {
        return;
    }

    if (fmt->count >= INT_MAX) {
        fmt->error = true;
        return;
    }

    if (fmt->size > 0) {
        if (fmt->count < fmt->size - 1) {
            *fmt->buffer++ = ch;
        } else {
            // write '\0' and enable counting mode
            *fmt->buffer = '\0';
            fmt->size = 0;
            fmt->write_fn = NULL;
        }
    } else if (fmt->write_fn != NULL) {
        if (!fmt->write_fn(ch)) {
            fmt->error = true;
            return;
        }
    }

    fmt->count++;
}

static void write_repeat(struct fmt *restrict fmt, char ch, int count) {
    for (int i = 0; i < count; i++) {
        write_char(fmt, ch);
        if (fmt->error) {
            return;
        }
    }
}

static void write_bytes(struct fmt *restrict fmt, const char *s, int len) {
    for (int i = 0; i < len; i++) {
        write_char(fmt, s[i]);
        if (fmt->error) {
            return;
        }
    }
}

static int utoa_rev(uintmax_t value, unsigned base, bool upper, char *out_rev) {
    static const char lo[] = "0123456789abcdef";
    static const char hi[] = "0123456789ABCDEF";
    const char *digits = upper ? hi : lo;
    int len = 0;

    if (value == 0) {
        out_rev[len++] = '0';
        return len;
    }

    while (value != 0) {
        out_rev[len++] = digits[value % base];
        value /= base;
    }
    return len;
}

static int get_width_or_precision(const char **fmt_ptr, va_list *arg, bool *ok) {
    int value = 0;
    const char *fmt = *fmt_ptr;

    if (*fmt == '*') {
        value = va_arg(*arg, int);
        fmt++;
        *fmt_ptr = fmt;
        return value;
    }

    while (isdigit(*fmt)) {
        int digit = *fmt - '0';
        if (value > (INT_MAX - digit) / 10) {
            *ok = false;
            return 0;
        }
        value = value * 10 + digit;
        fmt++;
    }
    *fmt_ptr = fmt;
    return value;
}

static intmax_t read_signed_arg(va_list *arg, enum length_modifier len) {
    switch (len) {
        case LEN_HH:
            return (signed char)va_arg(*arg, int);
        case LEN_H:
            return (short)va_arg(*arg, int);
        case LEN_L:
            return va_arg(*arg, long);
        case LEN_LL:
            return va_arg(*arg, long long);
        case LEN_J:
            return va_arg(*arg, intmax_t);
        case LEN_Z:
            return (intmax_t)va_arg(*arg, size_t);
        case LEN_T:
            return va_arg(*arg, ptrdiff_t);
        default:
            return va_arg(*arg, int);
    }
}

static uintmax_t read_unsigned_arg(va_list *arg, enum length_modifier len) {
    switch (len) {
        case LEN_HH:
            return (unsigned char)va_arg(*arg, unsigned);
        case LEN_H:
            return (unsigned short)va_arg(*arg, unsigned);
        case LEN_L:
            return va_arg(*arg, unsigned long);
        case LEN_LL:
            return va_arg(*arg, unsigned long long);
        case LEN_J:
            return va_arg(*arg, uintmax_t);
        case LEN_Z:
            return va_arg(*arg, size_t);
        case LEN_T:
            return (uintmax_t)va_arg(*arg, ptrdiff_t);
        default:
            return va_arg(*arg, unsigned);
    }
}

static void write_integer(struct fmt *restrict fmt,
    uintmax_t mag,
    bool negative,
    struct printf_flags flags,
    unsigned base,
    bool upper,
    char spec
) {
    char rev[64];
    int digits_len;

    int prefix_len = 0;
    char prefix[2];

    char sign = '\0';
    int zeroes = 0;
    int total_len;
    int pad_len;

    if (negative) {
        sign = '-';
    } else if (flags.plus) {
        sign = '+';
    } else if (flags.space) {
        sign = ' ';
    }

    if (flags.precision == 0 && mag == 0) {
        digits_len = 0;
    } else {
        digits_len = utoa_rev(mag, base, upper, rev);
    }

    if (flags.alt) {
        if (base == 8) {
            if (!(digits_len > 0 && rev[digits_len - 1] == '0')) {
                // Add leading '0' by increasing precision, if necessary.
                if (digits_len == 0) {
                    // Special case: insert "0" instead of increasing precision.
                    rev[0] = '0';
                    digits_len = 1;
                } else if (flags.precision <= digits_len) {
                    flags.precision = digits_len + 1;
                }
            }
        } else if ((base == 16) && (mag != 0)) {
            prefix[prefix_len++] = '0';
            prefix[prefix_len++] = (spec == 'X') ? 'X' : 'x';
        }
    }

    if (flags.precision > digits_len) {
        zeroes = flags.precision - digits_len;
    }

    total_len = digits_len + zeroes + prefix_len + (sign ? 1 : 0);

    if (flags.precision < 0 && flags.zero_pad && !flags.left_adj) {
        if (flags.width > total_len) {
            zeroes += flags.width - total_len;
            total_len = flags.width;
        }
    }

    pad_len = (flags.width > total_len) ? (flags.width - total_len) : 0;

    if (!flags.left_adj) {
        write_repeat(fmt, ' ', pad_len);
    }

    if (sign) {
        write_char(fmt, sign);
    }
    for (int i = 0; i < prefix_len; i++) {
        write_char(fmt, prefix[i]);
    }
    write_repeat(fmt, '0', zeroes);

    for (int i = 0; i < digits_len; i++) {
        write_char(fmt, rev[digits_len - 1 - i]);
    }

    if (flags.left_adj) {
        write_repeat(fmt, ' ', pad_len);
    }
}

int fmt_sprintf(struct fmt *restrict fmt, const char *restrict format, ...) {
    va_list arg;
    va_start(arg, format);
    int result = fmt_vsprintf(fmt, format, arg);
    va_end(arg);
    return result;
}

int fmt_vsprintf(struct fmt *restrict fmt, const char *restrict format, va_list arg) {
    if (fmt == NULL) {
        return -1;
    }

    if (fmt->size > INT_MAX) {
        return -1;
    }

    if (format == NULL) {
        if (fmt->size > 0) {
            *fmt->buffer = '\0';
        }
        return -1;
    }

    const char *cursor = format;
    va_list ap;
    va_copy(ap, arg);

    while (*cursor != '\0') {
        struct printf_flags flags = {
            .left_adj = false,
            .plus = false,
            .space = false,
            .alt = false,
            .zero_pad = false,
            .width = 0,
            .precision = -1
        };
        enum length_modifier len = LEN_NONE;
        bool ok = true;
        char spec;

        if (*cursor != '%') {
            write_char(fmt, *cursor++);
            if (fmt->error) {
                goto error;
            }
            continue;
        }

        cursor++;
        if (*cursor == '%') {
            write_char(fmt, *cursor++);
            if (fmt->error) {
                goto error;
            }
            continue;
        }

        // modifiers
        while (1) {
            if (*cursor == '-') {
                flags.left_adj = true;
            } else if (*cursor == '+') {
                flags.plus = true;
            } else if (*cursor == ' ') {
                flags.space = true;
            } else if (*cursor == '#') {
                flags.alt = true;
            } else if (*cursor == '0') {
                flags.zero_pad = true;
            } else {
                break;
            }
            cursor++;
        }

        // width
        if (*cursor == '*' || isdigit(*cursor)) {
            flags.width = get_width_or_precision(&cursor, &ap, &ok);
            if (!ok) {
                // ignore invalid conversion
                continue;
            }
            if (flags.width < 0) {
                flags.left_adj = true;
                flags.width = -flags.width;
            }
        }

        // precision
        if (*cursor == '.') {
            cursor++;
            flags.precision = get_width_or_precision(&cursor, &ap, &ok);
            if (!ok) {
                // ignore invalid conversion
                continue;
            }
            if (flags.precision < 0) {
                flags.precision = -1;
            }
            flags.zero_pad = false;
        }

        // length
        if (*cursor == 'h') {
            cursor++;
            if (*cursor == 'h') {
                len = LEN_HH;
                cursor++;
            } else {
                len = LEN_H;
            }
        } else if (*cursor == 'l') {
            cursor++;
            if (*cursor == 'l') {
                len = LEN_LL;
                cursor++;
            } else {
                len = LEN_L;
            }
        } else if (*cursor == 'j') {
            len = LEN_J;
            cursor++;
        } else if (*cursor == 'z') {
            len = LEN_Z;
            cursor++;
        } else if (*cursor == 't') {
            len = LEN_T;
            cursor++;
        } else if (*cursor == 'L') {
            len = LEN_CAP_L;
            cursor++;
        }

        // specifier
        spec = *cursor;
        if (spec == '\0') {
            // ignore incomplete conversion
            break;
        }
        cursor++;

        switch (spec) {
            case 'd':
            case 'i': {
                intmax_t value = read_signed_arg(&ap, len);
                bool negative = value < 0;
                uintmax_t mag;

                if (negative) {
                    mag = (uintmax_t) (-(value + 1)) + 1;
                } else {
                    mag = (uintmax_t) value;
                }

                write_integer(fmt, mag, negative, flags, 10, false, spec);
                break;
            }
            case 'u':
                write_integer(fmt, read_unsigned_arg(&ap, len), false, flags, 10, false, spec);
                break;
            case 'o':
                write_integer(fmt, read_unsigned_arg(&ap, len), false, flags, 8, false, spec);
                break;
            case 'x':
            case 'X':
                write_integer(fmt, read_unsigned_arg(&ap, len), false, flags, 16, spec == 'X', spec);
                break;
            case 'c': {
                char ch = (char) va_arg(ap, int);
                if (!flags.left_adj) {
                    write_repeat(fmt, ' ', flags.width > 1 ? flags.width - 1 : 0);
                }
                write_char(fmt, ch);
                if (flags.left_adj) {
                    write_repeat(fmt, ' ', flags.width > 1 ? flags.width - 1 : 0);
                }
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                int len_s;
                int pad;

                if (s == NULL) {
                    goto error;
                }

                if (flags.precision >= 0) {
                    len_s = strnlen_s(s, flags.precision);
                } else {
                    len_s = strlen(s);
                }

                pad = (flags.width > len_s) ? (flags.width - len_s) : 0;
                if (!flags.left_adj) {
                    write_repeat(fmt, ' ', pad);
                }
                write_bytes(fmt, s, len_s);
                if (flags.left_adj) {
                    write_repeat(fmt, ' ', pad);
                }
                break;
            }
            case 'p': {
                uintptr_t ptr = (uintptr_t)va_arg(ap, void *);
                write_integer(fmt, (uintmax_t)ptr, false, (struct printf_flags){
                    .left_adj = flags.left_adj,
                    .plus = false,
                    .space = false,
                    .alt = true,
                    .zero_pad = false,
                    .width = flags.width,
                    .precision = flags.precision
                }, 16, false, 'x');
                break;
            }
            case 'n':
                // 'n' is not allowed
                goto error;
            case 'a':
            case 'A':
            case 'e':
            case 'E':
            case 'f':
            case 'F':
            case 'g':
            case 'G':
                // floating point not supported; ignore conversion
                break;
            default:
                // unknown specifier; ignore invalid conversion
                break;
        }

        // check errors for write_integer and others
        if (fmt->error) {
            goto error;
        }
    }

    if (fmt->size > 0) {
        *fmt->buffer = '\0';
    }

    va_end(ap);
    return (int)fmt->count;

error:
    va_end(ap);
    return -1;
}
