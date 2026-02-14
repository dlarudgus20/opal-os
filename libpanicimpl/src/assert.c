#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdnoreturn.h>
#include <signal.h>

noreturn void panic_format(const char *msg, const char *file, const char *func, unsigned line, ...) {
    va_list args;
    va_start(args, line);
    fprintf(stderr, "[%s:%s:%d] ", file, func, line);
    vfprintf(stderr, msg, args);
    fputc('\n', stderr);
    raise(SIGINT);
    abort();
}
