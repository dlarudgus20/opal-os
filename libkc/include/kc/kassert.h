#ifndef KC_KASSERT_H
#define KC_KASSERT_H

#include "attributes.h"

#define panic(msg) (_panic_format("%s", __FILE__, __func__, __LINE__, (#msg)[0] ? "panic : " msg : "panic"))

#define KASSERT_1(exp, ...) ((void)((exp) || (panic("assertion failed : " #exp), 1)))
#define KASSERT_2(exp, msg) ((void)((exp) || (panic(msg " : " #exp), 1)))

#define KASSERT_EXPAND(exp, msg, dummy, impl, ...) impl(exp, msg)
#define kassert(...) KASSERT_EXPAND(__VA_ARGS__, , KASSERT_2, KASSERT_1, )

#define panicf(msg, ...) (_panic_format("panic : " msg, __FILE__, __func__, __LINE__, __VA_ARGS__))
#define kassertf(exp, msg, ...) ((void)((exp) || (_panic_format(msg " : %s", __FILE__, __func__, __LINE__, __VA_ARGS__, #exp), 1)))

[[noreturn]] void _panic_format(const char *fmt, const char *file, const char *func, unsigned line, ...) PRINTF_ATTR(1, 5);

#endif
