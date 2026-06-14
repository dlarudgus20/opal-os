#ifndef LIBUC_H
#define LIBUC_H

#include <stddef.h>

#define FD_INVALID -1

enum open_mode {
    OPEN_NONE = 0,
    OPEN_READ = 0x01,
    OPEN_WRITE = 0x02,
    OPEN_APPEND = 0x04,
};

typedef int pid_t;

[[noreturn]] void task_exit(void);
int putchar(int ch);
int getchar(void);
int puts(const char *str);
size_t getline(char *buf, size_t len);
int printf(const char *msg, ...);
int open(int fd, const char *path, enum open_mode mode);
int close(int fd);
int dup(int oldfd, int newfd);
int readc(int fd);
int writec(int fd, int ch);
long ioctl(int fd, unsigned long op, unsigned long arg);
int mount(const char *fstype, int arg, const char *path);

int pipe(int fds[2]);
pid_t fork(void);
int exec(int fd);

#endif
