#include <stdint.h>

#include <kc/string.h>
#include <kc/assert.h>

#include <opal/dynarray.h>
#include <opal/mm/kmalloc.h>

void dynarray_init(struct dynarray *ar) {
    ar->data = NULL;
    ar->capacity = 0;
    ar->size = 0;
}

void dynarray_destroy(struct dynarray *ar) {
    if (ar->data) {
        kfree(ar->data, ar->capacity);
        ar->capacity = ar->size = 0;
        ar->data = NULL;
    }
}

static bool reserve_without_copy(struct dynarray *ar, size_t new_capacity, struct span *old_data) {
    struct span new_data = kzalloc_span(new_capacity);
    if (!new_data.ptr) {
        return false;
    }

    *old_data = SPAN(ar->data, ar->capacity);
    ar->data = new_data.ptr;
    ar->capacity = new_data.size;
    return true;
}

static bool reserve_unchecked(struct dynarray *ar, size_t new_capacity) {
    struct span old_data;
    if (!reserve_without_copy(ar, new_capacity, &old_data)) {
        return false;
    }

    if (old_data.ptr) {
        if (ar->size > 0) {
            memcpy(ar->data, old_data.ptr, ar->size);
        }
        kfree_span(old_data);
    }
    return true;
}

bool dynarray_reserve(struct dynarray *ar, size_t new_capacity) {
    if (new_capacity <= ar->capacity) {
        return true;
    }

    return reserve_unchecked(ar, new_capacity);
}

bool dynarray_resize(struct dynarray *ar, size_t new_size) {
    if (new_size > ar->capacity) {
        if (!dynarray_reserve(ar, new_size)) {
            return false;
        }
    }

    ar->size = new_size;
    return true;
}

bool dynarray_shrink_to(struct dynarray *ar, size_t new_size) {
    if (new_size == 0) {
        dynarray_destroy(ar);
        return true;
    }

    if (new_size > ar->capacity / 2) {
        return true;
    }

    if (!reserve_unchecked(ar, new_size)) {
        return false;
    }

    ar->size = new_size;
    return true;
}

void *dynarray_push_back(struct dynarray *ar, size_t data_size) {
    assert(data_size <= SIZE_MAX - ar->size, "dynarray: push_back size overflow");
    size_t size = ar->size;
    if (!dynarray_resize(ar, size + data_size)) {
        return NULL;
    }
    return (unsigned char *)ar->data + size;
}

void dynarray_pop_back(struct dynarray *ar, size_t data_size) {
    assert(data_size <= ar->size, "dynarray: pop_back out of bounds");
    ar->size -= data_size;
}

void *dynarray_insert(struct dynarray *ar, size_t pos, size_t data_size) {
    assert(pos <= ar->size, "dynarray: insert out of bounds");
    assert(data_size <= SIZE_MAX - ar->size, "dynarray: insert size overflow");

    struct span old_data = SPAN_NULL;
    size_t new_size = ar->size + data_size;
    if (new_size > ar->capacity) {
        if (!reserve_without_copy(ar, new_size, &old_data)) {
            return NULL;
        }
    }

    unsigned char *inserted = (unsigned char *)ar->data + pos;
    if (old_data.ptr) {
        if (pos > 0) {
            memcpy(ar->data, old_data.ptr, pos);
        }
        if (ar->size > pos) {
            memcpy(inserted + data_size, (unsigned char *)old_data.ptr + pos, ar->size - pos);
        }
        kfree_span(old_data);
    } else if (ar->size > pos) {
        memmove(inserted + data_size, inserted, ar->size - pos);
    }
    ar->size += data_size;
    return inserted;
}

void dynarray_remove(struct dynarray *ar, size_t pos, size_t data_size) {
    assert(pos + data_size <= ar->size, "dynarray: remove out of bounds");
    if (pos + data_size < ar->size) {
        memmove((unsigned char *)ar->data + pos, (unsigned char *)ar->data + pos + data_size, ar->size - pos - data_size);
    }
    ar->size -= data_size;
}
