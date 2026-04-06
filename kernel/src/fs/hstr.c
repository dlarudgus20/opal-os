#include <kc/string.h>

#include <opal/fs/hstr.h>
#include <opal/mm/kmalloc.h>

static uint32_t str_hash(const char *str, uint32_t len) {
    uint32_t hash = 5381;
    for (uint32_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (uint8_t)str[i];
    }
    return hash;
}

static bool is_short(const struct hstr *pstr) {
    return pstr->in_len != 0xff;
}

static uint32_t get_long_len(const struct hstr *pstr) {
    return pstr->len >> 8;
}

static uint32_t make_long_len(uint32_t len) {
    return (len << 8) | 0xff;
}

struct hstr hstr_alloc(size_t len) {
    if (len + 1 < HSTR_SHORT_LEN) {
        return (struct hstr){ .in_len = (uint8_t)len };
    }

    char *buf = kzalloc(len + 1);
    if (!buf) {
        return HSTR_NULL;
    }
    return (struct hstr){
        .len = make_long_len((uint32_t)len),
        .str = buf,
    };
}

void hstr_free(struct hstr *pstr) {
    if (!is_short(pstr)) {
        kfree((char *)pstr->str, get_long_len(pstr) + 1);
    }
}

struct hstr hstr_stack(const char *str, uint32_t len) {
    return (struct hstr){
        .hash = str_hash(str, len),
        .len = make_long_len(len),
        .str = str,
    };
}

bool hstr_is_null(const struct hstr *pstr) {
    return !is_short(pstr) && !pstr->str;
}

void hstr_rehash(struct hstr *pstr) {
    pstr->hash = str_hash(hstrget(pstr), hstrlen(pstr));
}

const char *hstrget(const struct hstr *pstr) {
    return is_short(pstr) ? pstr->in_str : pstr->str;
}

size_t hstrlen(const struct hstr *pstr) {
    return is_short(pstr) ? pstr->in_len : get_long_len(pstr);
}

struct hstr hstr_clone(const struct hstr *str) {
    if (is_short(str)) {
        return *str;
    }

    size_t len = hstrlen(str);
    struct hstr hs = hstr_alloc(len);
    if (hstr_is_null(&hs)) {
        return hs;
    }

    memcpy((char *)hstrget(&hs), hstrget(str), len);
    hs.hash = str->hash;
    return hs;
}

struct hstr hstrdup(const char *str) {
    size_t len = strlen(str);
    struct hstr hs = hstr_alloc(len);
    if (hstr_is_null(&hs)) {
        return hs;
    }

    memcpy((char *)hstrget(&hs), str, len);
    hstr_rehash(&hs);
    return hs;
}

bool hstr_equal(const struct hstr *a, const struct hstr *b) {
    size_t len = hstrlen(a);
    return len == hstrlen(b) && memcmp(hstrget(a), hstrget(b), len) == 0;
}
