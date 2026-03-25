#ifndef OPAL_DYNARRAY_H
#define OPAL_DYNARRAY_H

#include <stddef.h>

#include <kc/span.h>

#include <collections/singlylist.h>

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

#define dynarray_at(list, type, index) (*(type *)((unsigned char *)(list)->data + (index) * sizeof(type)))

#define dynarray_foreach(type, ptr, list) \
    for (type ptr = (type)(list)->data; \
        (list)->size != 0 && (unsigned char *)ptr < (unsigned char *)(list)->data + (list)->size; ptr++)

#endif
