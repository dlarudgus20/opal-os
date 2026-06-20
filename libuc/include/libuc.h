#ifndef LIBUC_H
#define LIBUC_H

#include <stddef.h>

#define FD_INVALID -1

enum {
    FD_STDIN = 0,
    FD_STDOUT = 1,
    FD_STDERR = 2,
};

enum open_mode {
    OPEN_NONE = 0,
    OPEN_READ = 0x01,
    OPEN_WRITE = 0x02,
    OPEN_APPEND = 0x04,

    OPEN_CREATE = 0x10,
    OPEN_NONEXIST = 0x20,
    OPEN_TRUNC = 0x40,

    OPEN_MASK_FMODE = 0x0f,
    OPEN_MASK_ALL = OPEN_READ | OPEN_WRITE | OPEN_APPEND | OPEN_CREATE | OPEN_NONEXIST | OPEN_TRUNC,
};

typedef int pid_t;
typedef long ssize_t;

[[noreturn]] void task_exit(void);
int putchar(int ch);
int getchar(void);
int puts(const char *str);
size_t getline(char *buf, size_t len);
int printf(const char *msg, ...);
int open(int fd, const char *path, enum open_mode mode);
int close(int fd);
int dup(int oldfd, int newfd);
ssize_t read(int fd, void *buffer, size_t size);
ssize_t write(int fd, const void *buffer, size_t size);
long ioctl(int fd, unsigned long op, unsigned long arg);
int mount(const char *fstype, int arg, const char *path);

int pipe(int fds[2]);

#endif
