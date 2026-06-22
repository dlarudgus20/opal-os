#ifndef LIBUC_H
#define LIBUC_H

#include <stddef.h>

enum {
    FD_STDIN = 0,
    FD_STDOUT = 1,
    FD_STDERR = 2,
};

int putchar(int ch);
int getchar(void);
int puts(const char *str);
size_t getline(char *buf, size_t len);
int printf(const char *msg, ...);

#endif
