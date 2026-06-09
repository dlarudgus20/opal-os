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
};

void slab_create(struct slab *slab, size_t object_size, size_t object_align);
void slab_destroy(struct slab *slab);

[[nodiscard]] void *slab_alloc(struct slab *slab);
void slab_free(struct slab *slab, void *ptr);

[[nodiscard]] size_t slab_get_object_size(const struct slab *slab);
[[nodiscard]] size_t slab_get_inuse(const struct slab *slab);
[[nodiscard]] size_t slab_get_total(const struct slab *slab);

#define slab_create_for(slab, type) slab_create(slab, sizeof(type), alignof(type))

#endif
