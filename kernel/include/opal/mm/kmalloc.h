#ifndef OPAL_MM_KMALLOC_H
#define OPAL_MM_KMALLOC_H

#include <stddef.h>

void kmalloc_init(void);
[[nodiscard]] void *kmalloc(size_t size);
void kfree(void *ptr, size_t size);

#endif
