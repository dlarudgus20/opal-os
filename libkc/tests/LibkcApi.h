#ifndef LIBKCAPI_H
#define LIBKCAPI_H

#include <stddef.h>

using memcpy_fn = void *(*)(void *, const void *, size_t);
using memmove_fn = void *(*)(void *, const void *, size_t);
using memset_fn = void *(*)(void *, int, size_t);
using memcmp_fn = int (*)(const void *, const void *, size_t);
using strlen_fn = size_t (*)(const char *);
using strspn_fn = size_t (*)(const char *, const char *);
using strchr_fn = char *(*)(const char *, int);
using strnchr_fn = char *(*)(const char *, int, size_t);
using strcmp_fn = int (*)(const char *, const char *);
using strncmp_fn = int (*)(const char *, const char *, size_t);
using strcpy_fn = char *(*)(char *, const char *);
using strncpy_fn = char *(*)(char *, const char *, size_t);

struct LibkcApi {
    void *handle;
    memcpy_fn memcpy_ptr;
    memmove_fn memmove_ptr;
    memset_fn memset_ptr;
    memcmp_fn memcmp_ptr;
    strlen_fn strlen_ptr;
    strspn_fn strspn_ptr;
    strchr_fn strchr_ptr;
    strnchr_fn strnchr_ptr;
    strcmp_fn strcmp_ptr;
    strncmp_fn strncmp_ptr;
    strcpy_fn strcpy_ptr;
    strncpy_fn strncpy_ptr;
};

LibkcApi load_api();
void unload_api(LibkcApi &api);

#endif
