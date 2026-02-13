#ifndef KC_ASSERT_H
#define KC_ASSERT_H

#include <stdnoreturn.h>
#include "attributes.h"

#define panic(msg) (panic_format("%s", __FILE__, __func__, __LINE__, (#msg)[0] ? "panic : " msg : "panic"))

#define ASSERT_1(exp, ...) ((void)((exp) || (panic_format("%s", __FILE__, __func__, __LINE__, "assertion failed : " #exp), 1)))
#define ASSERT_2(exp, msg) ((void)((exp) || (panic_format("%s", __FILE__, __func__, __LINE__, msg " : " #exp), 1)))

#define ASSERT_EXPAND(exp, msg, dummy, impl, ...) impl(exp, msg)
#define assert(...) ASSERT_EXPAND(__VA_ARGS__, , ASSERT_2, ASSERT_1, )

#define panicf(msg, ...) (panic_format("panic : " msg, __FILE__, __func__, __LINE__, __VA_ARGS__))
#define assertf(exp, msg, ...) ((void)((exp) || (panic_format(msg " : %s", __FILE__, __func__, __LINE__, __VA_ARGS__, #exp), 1)))

noreturn void panic_format(const char *fmt, const char *file, const char *func, unsigned line, ...) PRINTF_ATTR(1, 5);

#endif
