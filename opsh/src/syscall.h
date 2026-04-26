#ifndef SYSCALL_H
#define SYSCALL_H

#include <stddef.h>

[[noreturn]] void task_exit(void);
int putchar(int ch);
int getchar(void);
int puts(const char *str);
size_t getline(char *buf, size_t len);
int printf(const char *msg, ...);
int open(const char *path);
int close(int fd);
int readc(int fd, size_t pos);

#endif
