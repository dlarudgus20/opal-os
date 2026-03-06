#ifndef OPAL_MM_SLAB_H
#define OPAL_MM_SLAB_H

#include <stddef.h>
#include <stdint.h>

#include <collections/linkedlist.h>

struct slab {
    uint16_t object_size;
    uint16_t object_align;

    uint16_t payload_offset;
    uint16_t slot_stride;

    uint32_t slot_offset;
    uint32_t page_capacity;

    uint32_t inuse_objects;
    uint32_t total_objects;

    struct linkedlist partial_pages;

    bool initialized;
};

void slab_create(struct slab *cache, size_t object_size, size_t object_align);
void slab_destroy(struct slab *cache);

void *slab_alloc(struct slab *cache);
void slab_free(struct slab *cache, void *ptr);

size_t slab_get_object_size(const struct slab *cache);
size_t slab_get_inuse(const struct slab *cache);
size_t slab_get_total(const struct slab *cache);

#endif
