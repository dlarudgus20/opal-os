#include <kc/string.h>
#include <kc/assert.h>
#include "collections/arraylist.h"

struct tagged_ptr {
    void* ptr;
    size_t len;
};

void arraylist_init(struct arraylist* list, size_t initial_capacity, const struct arraylist_allocator* pa) {
    list->pa = *pa;
    list->capacity = initial_capacity;
    list->size = 0;
    if (initial_capacity == 0) {
        list->data = NULL;
    } else {
        list->data = pa->alloc(initial_capacity, &list->capacity);
        assert(list->data, "arraylist: init allocation failed");
    }
}

static struct tagged_ptr reserve_without_copy(struct arraylist* list, size_t new_capacity) {
    if (new_capacity <= list->capacity) {
        return (struct tagged_ptr){ NULL, 0 };
    }
    size_t allocated_len;
    void* new_data = list->pa.alloc(new_capacity, &allocated_len);
    struct tagged_ptr old_data = { list->data, list->capacity };
    assert(new_data, "arraylist: out of memory");
    list->data = new_data;
    list->capacity = allocated_len;
    return old_data;
}

void arraylist_reserve(struct arraylist* list, size_t new_capacity) {
    struct tagged_ptr old_data = reserve_without_copy(list, new_capacity);
    if (old_data.ptr) {
        if (list->size > 0) {
            memcpy(list->data, old_data.ptr, list->size);
        }
        list->pa.dealloc(old_data.ptr, old_data.len);
    }
}

static size_t capacity_for_size(size_t new_size, size_t old_capacity) {
    return new_size > old_capacity * 2 ? new_size : old_capacity * 2;
}

void arraylist_resize(struct arraylist* list, size_t new_size) {
    if (new_size > list->capacity) {
        size_t new_capacity = capacity_for_size(new_size, list->capacity);
        arraylist_reserve(list, new_capacity);
    }
    list->size = new_size;
}

void arraylist_shrink_to(struct arraylist* list, size_t new_size) {
    if (new_size == 0) {
        if (list->data) {
            list->pa.dealloc(list->data, list->capacity);
            list->capacity = list->size = 0;
            list->data = NULL;
        }
    } else if (new_size < list->capacity) {
        list->capacity = list->pa.shrink(list->data, list->capacity, new_size);
        if (new_size < list->size) {
            list->size = new_size;
        }
    }
}

void* arraylist_push_back(struct arraylist* list, size_t data_size) {
    size_t size = list->size;
    arraylist_resize(list, size + data_size);
    return (char*)list->data + size;
}

void arraylist_pop_back(struct arraylist* list, size_t data_size) {
    assert(data_size <= list->size, "arraylist: pop_back out of bounds");
    list->size -= data_size;
}

void* arraylist_insert(struct arraylist* list, size_t pos, size_t data_size) {
    assert(pos <= list->size, "arraylist: insert out of bounds");

    struct tagged_ptr old_data = reserve_without_copy(list, list->size + data_size);
    char* inserted = (char*)list->data + pos;
    if (old_data.ptr) {
        if (pos > 0) {
            memcpy(list->data, old_data.ptr, pos);
        }
        if (list->size > pos) {
            memcpy(inserted + data_size, (char*)old_data.ptr + pos, list->size - pos);
        }
        list->pa.dealloc(old_data.ptr, old_data.len);
    } else if (list->size > pos) {
        memmove(inserted + data_size, inserted, list->size - pos);
    }
    list->size += data_size;
    return inserted;
}

void arraylist_remove(struct arraylist* list, size_t pos, size_t data_size) {
    assert(pos + data_size <= list->size, "arraylist: remove out of bounds");
    if (pos + data_size < list->size) {
        memmove((char*)list->data + pos, (char*)list->data + pos + data_size, list->size - pos - data_size);
    }
    list->size -= data_size;
}
