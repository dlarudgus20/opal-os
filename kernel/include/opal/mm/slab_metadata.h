#ifndef OPAL_MM_SLAB_METADATA_H
#define OPAL_MM_SLAB_METADATA_H

#include <stdint.h>

#include <collections/linkedlist.h>

#include <opal/attributes.h>
#include <opal/platform/mm/defines.h>

#define SLAB_REDZONE_SIZE 2
#define SLAB_REDZONE_PATTERN 0xfd
#define SLAB_UNUSED_PATTERN 0xcc

// page: [struct slab_page][object 1][object 2]...
// object: [struct slab_obj_hdr][redzone][payload][redzone]

struct PACKED slab_obj_hdr {
    bool is_free:1;
    unsigned next_free:15;
};

struct slab_page {
    struct linkedlist_link link;
    struct slab *owner;
    uint32_t inuse;
    uint16_t free_head;
};

static_assert(sizeof(struct slab_obj_hdr) == 2);
static_assert(sizeof(struct slab_page) < PAGE_SIZE);

#define ALIGN_UP(x, a)      (((x) + ((size_t)(a) - 1)) & ~((size_t)(a) - 1))
#define ALIGN_DOWN(x, a)    ((x) & ~((size_t)(a) - 1))

#define SLOT_PREFIX(a) \
    (ALIGN_UP(sizeof(struct slab_obj_hdr) + SLAB_REDZONE_SIZE, (a)))

#define SLOT_STRIDE(x, a) \
    (ALIGN_UP(SLOT_PREFIX(a) + (x) + SLAB_REDZONE_SIZE, (a)))

#define SLOT_OFFSET(a) \
    (ALIGN_UP(sizeof(struct slab_page), (a)))

#define SLAB_PAGE_USABLE(a) \
    (PAGE_SIZE - SLOT_OFFSET(a))

#define SLOTS_COUNT(x, a) \
    (SLAB_PAGE_USABLE(a) / SLOT_STRIDE((x), (a)))

#define SLOT_EXTEND(x, a) \
    (ALIGN_DOWN(SLAB_PAGE_USABLE(a) / SLOTS_COUNT((x), (a)), (a)) \
        - SLOT_PREFIX(a) - SLAB_REDZONE_SIZE)

#endif
