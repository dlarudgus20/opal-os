#ifndef OPAL_MM_KMALLOC_H
#define OPAL_MM_KMALLOC_H

#include <stddef.h>

#include <opal/platform/mm/defines.h>

#define KMALLOC_MAX_SIZE (PAGE_SIZE * 32)

void kmalloc_init(void);
[[nodiscard]] void *kmalloc(size_t size);
void kfree(void *ptr, size_t size);

#endif
