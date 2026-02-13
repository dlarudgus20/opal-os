#ifndef KC_STRING_H
#define KC_STRING_H

#include <stddef.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *dest, int ch, size_t n);
int memcmp(const void *lhs, const void *rhs, size_t n);

size_t strlen(const char *s);
size_t strnlen_s(const char *s, size_t strsz);
size_t strspn(const char *s, const char *accept);
char *strchr(const char *s, int ch);
char *strnchr(const char *s, int ch, size_t n);
int strcmp(const char *lhs, const char *rhs);
int strncmp(const char *lhs, const char *rhs, size_t n);
char *strcpy(char *restrict dest, const char *restrict src);
char *strncpy(char *restrict dest, const char *restrict src, size_t n);

#endif
