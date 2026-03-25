#include <kc/assert.h>
#include <kc/string.h>

#include <opal/mm/mm.h>
#include <opal/mm/pfn.h>
#include <opal/mm/slab.h>
#include <opal/mm/slab_metadata.h>
#include <opal/mm/kmalloc.h>
#include <opal/locks/irqlock.h>

#define KMALLOC_ALIGN 8

static const uint16_t g_slab_sizes[] = {
    SLOT_EXTEND(32, KMALLOC_ALIGN),
    SLOT_EXTEND(48, KMALLOC_ALIGN),
    SLOT_EXTEND(64, KMALLOC_ALIGN),
    SLOT_EXTEND(80, KMALLOC_ALIGN),
    SLOT_EXTEND(96, KMALLOC_ALIGN),
    SLOT_EXTEND(128, KMALLOC_ALIGN),
    SLOT_EXTEND(160, KMALLOC_ALIGN),
    SLOT_EXTEND(192, KMALLOC_ALIGN),
    SLOT_EXTEND(256, KMALLOC_ALIGN),
    SLOT_EXTEND(384, KMALLOC_ALIGN),
    SLOT_EXTEND(512, KMALLOC_ALIGN),
    SLOT_EXTEND(768, KMALLOC_ALIGN),
    SLOT_EXTEND(1024, KMALLOC_ALIGN),
};

#define KMALLOC_SLABS (sizeof(g_slab_sizes) / sizeof(g_slab_sizes[0]))

static struct kmalloc_slab_list g_slab_list = {
    .sizes = g_slab_sizes,
    .count = KMALLOC_SLABS,
};

static struct slab g_slabs[KMALLOC_SLABS];

static int slab_index_for_size(size_t size) {
    for (uint8_t i = 0; i < KMALLOC_SLABS; i++) {
        if (size <= g_slab_sizes[i]) {
            return i;
        }
    }
    return -1;
}

static uint8_t page_order_for_size(size_t size) {
    uint8_t order = 0;
    size_t bytes = PAGE_SIZE;
    while (order < PFN_VALID_BIT_WIDTH && bytes < size) {
        order++;
        bytes <<= 1;
    }
    return order;
}

void kmalloc_init(void) {
    for (uint8_t i = 0; i < KMALLOC_SLABS; i++) {
        slab_create(&g_slabs[i], g_slab_sizes[i], KMALLOC_ALIGN);
    }
}

const struct kmalloc_slab_list *kmalloc_get_slab_list(void) {
    return &g_slab_list;
}

struct span kzalloc_span(size_t size) {
    if (size == 0) {
        return SPAN_NULL;
    }

    assert(size <= KMALLOC_MAX_SIZE, "invalid size");

    int idx = slab_index_for_size(size);
    if (idx >= 0) {
        irqlock_t irqlock = irqlock_acquire();
        void *ptr = slab_alloc(&g_slabs[idx]);
        irqlock_release(&irqlock);
        return SPAN(ptr, g_slab_sizes[idx]);
    }

    const uint8_t order = page_order_for_size(size);
    assert(order < PFN_VALID_BIT_WIDTH, "invalid size");

    void *ptr = mm_alloc_page_ptr(order);
    if (!ptr) {
        return SPAN_NULL;
    }

    size_t alloc_size = PAGE_SIZE << order;
    memset(ptr, 0, alloc_size);
    return SPAN(ptr, alloc_size);
}

void *kzalloc(size_t size) {
    return kzalloc_span(size).ptr;
}

void kfree_span(struct span span) {
    kfree(span.ptr, span.size);
}

void kfree(void *ptr, size_t size) {
    if (ptr == NULL || size == 0) {
        return;
    }

    assert(size <= KMALLOC_MAX_SIZE, "invalid size");

    int idx = slab_index_for_size(size);
    if (idx >= 0) {
        irqlock_t irqlock = irqlock_acquire();
        slab_free(&g_slabs[idx], ptr);
        irqlock_release(&irqlock);
        return;
    }

    const uint8_t order = page_order_for_size(size);
    assert(order < PFN_VALID_BIT_WIDTH, "invalid size");

    mm_free_page_ptr(ptr, order);
}
