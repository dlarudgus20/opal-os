#ifndef OPALSYS_OPALSYS_H
#define OPALSYS_OPALSYS_H

#ifdef OPAL_KERNEL
#error opalsys/opalsys.h must not be included in kernel code
#endif

#include <stddef.h>

#include <opalsys/vfs.h>

#define PID_INVALID -1
#define FD_INVALID -1

typedef int pid_t;
typedef long ssize_t;

[[noreturn]] void task_exit(void);
int open(int fd, const char *path, enum open_mode mode, enum inode_flags flags);
int close(int fd);
int dup(int oldfd, int newfd);
int stat(int fd);
ssize_t read(int fd, void *buffer, size_t size);
ssize_t write(int fd, const void *buffer, size_t size);
long ioctl(int fd, unsigned long op, unsigned long arg);
int mount(const char *fstype, int arg, const char *path);

int pipe(int fds[2]);
pid_t fork(void);
int exec(int fd);

#endif
