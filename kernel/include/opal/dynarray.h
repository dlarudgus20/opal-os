#ifndef OPAL_DYNARRAY_H
#define OPAL_DYNARRAY_H

#include <stddef.h>

#include <kc/span.h>

struct dynarray {
    void *data;
    size_t size;
    size_t capacity;
};

void dynarray_init(struct dynarray *ar);
void dynarray_destroy(struct dynarray *ar);
bool dynarray_reserve(struct dynarray *ar, size_t new_capacity);
bool dynarray_resize(struct dynarray *ar, size_t new_size);
bool dynarray_shrink_to(struct dynarray *ar, size_t new_size);
[[nodiscard]] void *dynarray_push_back(struct dynarray *ar, size_t data_size);
void dynarray_pop_back(struct dynarray *ar, size_t data_size);
[[nodiscard]] void *dynarray_insert(struct dynarray *ar, size_t pos, size_t data_size);
void dynarray_remove(struct dynarray *ar, size_t pos, size_t data_size);

#define dynarray_len(ar, type) ((ar)->size / sizeof(type))

#define dynarray_at(ar, type, index) (*(type *)((unsigned char *)(ar)->data + (index) * sizeof(type)))

#define dynarray_remove_at(ar, type, index) dynarray_remove((ar), (index) * sizeof(type), sizeof(type))

#ifdef __cplusplus
#define CXX_CAST(type, x) (type)(x)
#else
#define CXX_CAST(type, x) (x)
#endif

#define dynarray_foreach(type, ptr, ar) \
    for (type ptr = CXX_CAST(type, (ar)->data); \
        (ar)->size != 0 && (ptr) < (type)(ar)->data + (ar)->size / sizeof(*(type)0); ptr++)

#endif
