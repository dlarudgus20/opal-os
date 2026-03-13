#include <kc/assert.h>
#include <kc/string.h>

#include <opal/klog.h>
#include <opal/mm/tmpalloc.h>
#include <opal/platform/mm/defines.h>

void tmpalloc_create(struct tmpalloc *ta, struct mmap_entry *buffer, size_t len, const struct mmap *src) {
    ta->mm.length = src->length;
    if (ta->mm.length > len) {
        kwarn("tmpalloc: buffer is smaller than source mmap");
        ta->mm.length = len;
    }

    memcpy(buffer, src->entries, ta->mm.length * sizeof(struct mmap_entry));
    ta->mm.entries = buffer;
}

phys_addr_t tmpalloc_alloc_pages(struct tmpalloc *ta, size_t max_pages, size_t *allocated_pages) {
    assert(allocated_pages);
    assert(ta->mm.entries, "tmp_alloc is already finalized");

    for (uint32_t i = 0; i + 1 < ta->mm.length; i++) {
        struct mmap_entry *const entry = &ta->mm.entries[i];
        struct mmap_entry *const next = &ta->mm.entries[i + 1];

        if (entry->type == MM_SEC_ENTRY_USABLE) {
            panic("mm_section is corrupted");
        }

        if (next->len < PAGE_SIZE) {
            next->type = MM_SEC_ENTRY_METADATA;
            continue;
        }

        phys_size_t allocated = next->len / PAGE_SIZE;
        if (allocated > max_pages) {
            allocated = max_pages;
        }

        const phys_addr_t addr = next->addr;

        entry->len += allocated * PAGE_SIZE;
        next->addr += allocated * PAGE_SIZE;
        next->len -= allocated * PAGE_SIZE;
        *allocated_pages = allocated;
        return addr;
    }

    panic("mm_section is already full of metadata page");
}
