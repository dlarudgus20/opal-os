#ifndef OPAL_MM_KMALLOC_H
#define OPAL_MM_KMALLOC_H

#include <stddef.h>
#include <stdint.h>

#include <kc/span.h>

#include <opal/platform/mm/defines.h>

#define KMALLOC_MAX_SIZE (PAGE_SIZE * 32)
#define KMALLOC_ALIGN 8

struct kmalloc_slab_list {
    const uint16_t *sizes;
    size_t count;
};

void kmalloc_init(void);
[[nodiscard]] const struct kmalloc_slab_list *kmalloc_get_slab_list(void);

[[nodiscard]] struct span kzalloc_span(size_t size);
[[nodiscard]] void *kzalloc(size_t size);
void kfree_span(struct span span);
void kfree(void *ptr, size_t size);

#endif
