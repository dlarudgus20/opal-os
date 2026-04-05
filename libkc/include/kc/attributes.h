#ifndef KC_ATTRIBUTES_H
#define KC_ATTRIBUTES_H

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#if __has_attribute(format)
#define PRINTF_ATTR(a, b) [[gnu::format(printf, a, b)]]
#else
#define PRINTF_ATTR(a, b)
#endif

#if __has_attribute(always_inline)
#define ALWAYS_INLINE [[gnu::always_inline]] static inline
#else
#define ALWAYS_INLINE static inline
#endif

#if __has_attribute(assume)
#define ASSUME_ATTR(x) [[gnu::assume(x)]]
#else
#define ASSUME_ATTR(x)
#endif

#endif
