#include <kc/assert.h>

#include <opal/mm/mm.h>
#include <opal/mm/pfn.h>
#include <opal/mm/slab.h>
#include <opal/mm/kmalloc.h>
#include <opal/locks/irqlock.h>

#define SLAB_MIN_POW    5   // 32 to 1024
#define SLAB_MAX_POW    10

#define SLAB_ORDERS     (SLAB_MAX_POW - SLAB_MIN_POW + 1)

static struct slab g_slabs[SLAB_ORDERS];

static size_t slab_size_for_index(uint8_t idx) {
    return (size_t)(1 << SLAB_MIN_POW) << idx;
}

static size_t max_slab_size(void) {
    return slab_size_for_index(SLAB_ORDERS - 1);
}

static uint8_t slab_index_for_size(size_t size) {
    uint8_t idx = 0;
    size_t slab_size = slab_size_for_index(0);
    while (idx + 1 < SLAB_ORDERS && slab_size < size) {
        idx++;
        slab_size <<= 1;
    }
    return idx;
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
    for (uint8_t i = 0; i < SLAB_ORDERS; i++) {
        slab_create(&g_slabs[i], slab_size_for_index(i), 8);
    }
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    assert(size <= KMALLOC_MAX_SIZE, "invalid size");

    const size_t slab_max = max_slab_size();
    if (size <= slab_max) {
        const uint8_t idx = slab_index_for_size(size);

        irqlock_t irqlock = irqlock_acquire();
        void *ptr = slab_alloc(&g_slabs[idx]);
        irqlock_release(&irqlock);

        return ptr;
    }

    const uint8_t order = page_order_for_size(size);
    assert(order < PFN_VALID_BIT_WIDTH, "invalid size");

    pfn_t pfn = mm_alloc_page(order);
    if (pfn == PFN_INVALID) {
        return NULL;
    }

    return mm_pfn_to_ptr(pfn);
}

void kfree(void *ptr, size_t size) {
    if (ptr == NULL || size == 0) {
        return;
    }

    assert(size <= KMALLOC_MAX_SIZE, "invalid size");

    const size_t slab_max = max_slab_size();
    if (size <= slab_max) {
        const uint8_t idx = slab_index_for_size(size);

        irqlock_t irqlock = irqlock_acquire();
        slab_free(&g_slabs[idx], ptr);
        irqlock_release(&irqlock);

        return;
    }

    const uint8_t order = page_order_for_size(size);
    assert(order < PFN_VALID_BIT_WIDTH, "invalid size");

    mm_free_page(mm_ptr_to_pfn(ptr), order);
}
