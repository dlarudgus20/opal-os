#ifndef KC_STRING_H
#define KC_STRING_H

#include <stddef.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *dest, int ch, size_t n);
int memcmp(const void *lhs, const void *rhs, size_t n);
void* memchr(const void* ptr, int ch, size_t count);
void* memchr_not(const void* ptr, int ch, size_t count);

size_t strlen(const char *s);
size_t strnlen_s(const char *s, size_t strsz);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strchr(const char *s, int ch);
char *strrchr(const char *s, int ch);
char *strnchr(const char *s, int ch, size_t n);
int strcmp(const char *lhs, const char *rhs);
int strncmp(const char *lhs, const char *rhs, size_t n);
char *strcpy(char *restrict dest, const char *restrict src);
void strscpy(char *restrict dest, size_t destsz, const char *restrict src);
void strsncpy(char *restrict dest, size_t destsz, const char *restrict src, size_t n);
char *strcat(char *restrict dest, const char *restrict src);
char *strncat(char *restrict dest, const char *restrict src, size_t n);
void strscat(char *restrict dest, size_t destsz, const char *restrict src);

#ifndef NO_BUILTIN_MACRO
#define memcpy(dest, src, count)    __builtin_memcpy(dest, src, count)
#define memmove(dest, src, count)   __builtin_memmove(dest, src, count)
#define memset(dest, ch, count)     __builtin_memset(dest, ch, count)
#define memcmp(lhs, rhs, count)     __builtin_memcmp(lhs, rhs, count)
#define memchr(ptr, ch, count)      __builtin_memchr(ptr, ch, count)

#define strlen(s)                   __builtin_strlen(s)
#define strspn(s, accept)           __builtin_strspn(s, accept)
#define strcspn(s, reject)          __builtin_strcspn(s, reject)
#define strchr(s, ch)               __builtin_strchr(s, ch)
#define strrchr(s, ch)              __builtin_strrchr(s, ch)
#define strcmp(lhs, rhs)            __builtin_strcmp(lhs, rhs)
#define strncmp(lhs, rhs, n)        __builtin_strncmp(lhs, rhs, n)
#define strcpy(dest, src)           __builtin_strcpy(dest, src)
#define strcat(dest, src)           __builtin_strcat(dest, src)
#define strncat(dest, src, n)       __builtin_strncat(dest, src, n)
#endif

#endif
