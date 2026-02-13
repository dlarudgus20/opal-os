#ifndef KC_ATTRIBUTES_H
#define KC_ATTRIBUTES_H

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if __has_attribute(format)
#define PRINTF_ATTR(a, b) __attribute__((format(printf, a, b)))
#else
#define PRINTF_ATTR(a, b)
#endif

#endif
