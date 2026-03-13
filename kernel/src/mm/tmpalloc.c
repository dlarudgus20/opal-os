#include <kc/assert.h>
#include <kc/string.h>

#include <opal/klog.h>
#include <opal/mm/tmpalloc.h>
#include <opal/platform/mm/defines.h>

void tmpalloc_create(struct tmpalloc *ta, struct mmap_entry *buffer, size_t len, const struct mmap *src) {
    ta->capacity = len;
    ta->mm.length = src->length;
    if (ta->mm.length > len) {
        kwarn("tmpalloc: buffer is smaller than source mmap");
        ta->mm.length = len;
    }

    memcpy(buffer, src->entries, ta->mm.length * sizeof(struct mmap_entry));
    ta->mm.entries = buffer;
}

static void insert_entry(struct tmpalloc *ta, uint32_t index, struct mmap_entry entry) {
    if (ta->mm.length >= ta->capacity) {
        panic("too many mmap entries");
    }

    const size_t tail_count = ta->mm.length - index;
    memmove(&ta->mm.entries[index + 1], &ta->mm.entries[index], tail_count * sizeof(struct mmap_entry));
    ta->mm.entries[index] = entry;
    ta->mm.length++;
}

static void erase_entry(struct tmpalloc *ta, uint32_t index) {
    const size_t tail_count = ta->mm.length - index - 1;
    if (tail_count > 0) {
        memmove(&ta->mm.entries[index], &ta->mm.entries[index + 1], tail_count * sizeof(struct mmap_entry));
    }
    ta->mm.length--;
}

phys_addr_t tmpalloc_alloc_pages(struct tmpalloc *ta, size_t max_pages, size_t *allocated_pages) {
    assert(allocated_pages);
    assert(ta->mm.entries, "tmp_alloc is already finalized");

    for (uint32_t i = 0; i < ta->mm.length; i++) {
        struct mmap_entry *usable = &ta->mm.entries[i];
        if (usable->type != MM_SEC_ENTRY_USABLE) {
            continue;
        }

        if (usable->len < PAGE_SIZE) {
            continue;
        }

        phys_size_t allocated = usable->len / PAGE_SIZE;
        if (allocated > max_pages) {
            allocated = max_pages;
        }
        if (allocated == 0) {
            continue;
        }

        const phys_size_t allocated_len = allocated * PAGE_SIZE;
        const phys_addr_t addr = usable->addr;

        if (i > 0) {
            struct mmap_entry *prev = &ta->mm.entries[i - 1];
            if (prev->type == MM_SEC_ENTRY_METADATA && prev->addr + prev->len == usable->addr) {
                prev->len += allocated_len;
                goto inserted;
            }
        }

        insert_entry(ta, i, (struct mmap_entry){
            .addr = usable->addr,
            .len = allocated_len,
            .type = MM_SEC_ENTRY_METADATA,
        });
        i++;
        usable = &ta->mm.entries[i];

inserted:
        usable->addr += allocated_len;
        usable->len -= allocated_len;
        if (usable->len == 0) {
            erase_entry(ta, i);
        }

        *allocated_pages = allocated;
        return addr;
    }

    panic("mm_section has no usable page for tmpalloc");
}
