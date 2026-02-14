#pragma once

#include <stddef.h>

struct arraylist_allocator {
    void* (*alloc)(size_t len, size_t* allocated_len);
    void (*dealloc)(void* ptr, size_t len);
    size_t (*shrink)(void* ptr, size_t old_len, size_t new_len);
};

struct arraylist {
    struct arraylist_allocator pa;
    void* data;
    size_t size;
    size_t capacity;
};

void arraylist_init(struct arraylist* list, size_t initial_capacity, const struct arraylist_allocator* pa);
void arraylist_reserve(struct arraylist* list, size_t new_capacity);
void arraylist_resize(struct arraylist* list, size_t new_size);
void arraylist_shrink_to(struct arraylist* list, size_t new_size);
void* arraylist_push_back(struct arraylist* list, size_t data_size);
void arraylist_pop_back(struct arraylist* list, size_t data_size);
void* arraylist_insert(struct arraylist* list, size_t pos, size_t data_size);
void arraylist_remove(struct arraylist* list, size_t pos, size_t data_size);

#define arraylist_at(list, type, index) (*(type*)((char*)(list)->data + (index) * sizeof(type)))

#define arraylist_foreach(type, ptr, list) \
    for (type ptr = (type)(list)->data; \
        (list)->size != 0 && (char*)ptr < (char*)(list)->data + (list)->size; ptr++)
