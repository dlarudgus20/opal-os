#ifndef KC_CTYPE_H
#define KC_CTYPE_H

static inline int isdigit(char ch) {
    return ch >= '0' && ch <= '9';
}

static inline int isspace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

#endif
