#ifndef LIBKCAPI_H
#define LIBKCAPI_H

#include <stddef.h>

extern "C" {
void *kc_memcpy(void *, const void *, size_t);
void *kc_memmove(void *, const void *, size_t);
void *kc_memset(void *, int, size_t);
int kc_memcmp(const void *, const void *, size_t);
size_t kc_strlen(const char *);
size_t kc_strspn(const char *, const char *);
char *kc_strchr(const char *, int);
char *kc_strnchr(const char *, int, size_t);
int kc_strcmp(const char *, const char *);
int kc_strncmp(const char *, const char *, size_t);
char *kc_strcpy(char *, const char *);
char *kc_strncpy(char *, const char *, size_t);
int kc_snprintf_s(char *, size_t, const char *, ...);
}

#endif
