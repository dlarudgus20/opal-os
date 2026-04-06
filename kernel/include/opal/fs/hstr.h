#ifndef OPAL_FS_HSTR_H
#define OPAL_FS_HSTR_H

#include <stddef.h>
#include <stdint.h>

#define HSTR_SHORT_LEN 19

struct hstr {
    union {
        struct {
            uint32_t hash;
            uint32_t len;
            const char *str;
        };
        struct {
            uint32_t in_hash;
            uint8_t in_len;
            char in_str[HSTR_SHORT_LEN];
        };
    };
};

#define HSTR_NULL ((struct hstr){ .in_len = 0xff })
#define HSTR_EMPTY ((struct hstr){ .in_len = 0 })

struct hstr hstr_alloc(size_t size);
void hstr_free(struct hstr *pstr);

struct hstr hstr_stack(const char *str, uint32_t len);

bool hstr_is_null(const struct hstr *pstr);
void hstr_rehash(struct hstr *pstr);

const char *hstrget(const struct hstr *pstr);
size_t hstrlen(const struct hstr *pstr);

struct hstr hstr_clone(const struct hstr *str);
struct hstr hstrdup(const char *str);
bool hstr_equal(const struct hstr *a, const struct hstr *b);

#endif
