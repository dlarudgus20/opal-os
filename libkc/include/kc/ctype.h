#ifndef KC_CTYPE_H
#define KC_CTYPE_H

static inline int isdigit(int ch) {
    return ch >= '0' && ch <= '9';
}

static inline int isupper(int ch) {
    return ch >= 'A' && ch <= 'Z';
}

static inline int islower(int ch) {
    return ch >= 'a' && ch <= 'z';
}

static inline int isalpha(int ch) {
    return isupper(ch) || islower(ch);
}

static inline int isalnum(int ch) {
    return isdigit(ch) || isalpha(ch);
}

static inline int toupper(int ch) {
    return islower(ch) ? ch - 'a' + 'A' : ch;
}

static inline int tolower(int ch) {
    return isupper(ch) ? ch - 'A' + 'a' : ch;
}

static inline int isspace(int ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static inline int isprint(int ch) {
    return ch >= ' ' && ch <= '~';
}

#endif
