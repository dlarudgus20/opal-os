#ifndef LIBKC_TEST_H
#define LIBKC_TEST_H

struct LibkcApi {
    void load_api();
    void unload_api();
    void *handle;

    void *(*memcpy)(void *, const void *, size_t);
    void *(*memmove)(void *, const void *, size_t);
    void *(*memset)(void *, int, size_t);
    int (*memcmp)(const void *, const void *, size_t);
    size_t (*strlen)(const char *);
    size_t (*strspn)(const char *, const char *);
    size_t (*strcspn)(const char *, const char *);
    char *(*strchr)(const char *, int);
    char *(*strrchr)(const char *, int);
    char *(*strnchr)(const char *, int, size_t);
    int (*strcmp)(const char *, const char *);
    int (*strncmp)(const char *, const char *, size_t);
    char *(*strcpy)(char *, const char *);
    void (*strcpy_sized)(char *, size_t, const char *);
    void (*strncpy_sized)(char *, size_t, const char *, size_t);
    char *(*strcat)(char *, const char *);
    char *(*strncat)(char *, const char *, size_t);
    void (*strcat_sized)(char *, size_t, const char *);
    int (*snprintf_s)(char *, size_t, const char *, ...);
};

class LibkcTest : public ::testing::Test {
protected:
    static LibkcApi kc;

    static void SetUpTestSuite() {
        kc.load_api();
    }

    static void TearDownTestSuite() {
        kc.unload_api();
    }
};

#endif
