#include <kc/assert.h>
#include <kc/stdlib.h>

#include <opal/mm/vmap.h>
#include <opal/locks/irqlock.h>
#include <opal/platform/mm/pagetable.h>

#define MAX_VMAP_ENTRIES 128

struct vmap_entry {
    virt_addr_t addr;
    virt_addr_t len;
};

static struct vmap_entry g_vmap_entries[MAX_VMAP_ENTRIES];
static uint32_t g_vmap_len = 0;

static void remove_entry(uint32_t idx) {
    assert(idx < g_vmap_len);
    for (uint32_t i = idx + 1; i < g_vmap_len; i++) {
        g_vmap_entries[i - 1] = g_vmap_entries[i];
    }
    g_vmap_len--;
}

static void insert_entry(uint32_t idx, virt_addr_t addr, virt_size_t len) {
    assert(g_vmap_len < MAX_VMAP_ENTRIES);
    assert(idx <= g_vmap_len);

    for (uint32_t i = g_vmap_len; i > idx; i--) {
        g_vmap_entries[i] = g_vmap_entries[i - 1];
    }
    g_vmap_entries[idx] = (struct vmap_entry){
        .addr = addr,
        .len = len,
    };
    g_vmap_len++;
}

void mm_vmap_init(void) {
    g_vmap_entries[0] = (struct vmap_entry){
        .addr = VMAP_START_VIRT,
        .len = VMAP_END_VIRT - VMAP_START_VIRT,
    };
    g_vmap_len = 1;
}

struct span mm_vmap_alloc(void **va_out, phys_addr_t pa, phys_size_t size) {
    assert(va_out);
    *va_out = NULL;

    if (size == 0) {
        return (struct span){ .ptr = NULL, .size = 0 };
    }

    phys_addr_t aligned_start = align_floor_sz_p2(pa, PAGE_SIZE);
    phys_addr_t aligned_end = align_ceil_sz_p2(pa + size, PAGE_SIZE);
    assert(aligned_end == 0 || aligned_start < aligned_end, "overflow detected");

    phys_size_t aligned_size = aligned_end - aligned_start;
    if (aligned_size == 0) {
        return (struct span){ .ptr = 0, .size = 0 };
    }

    irqlock_t irqlock = irqlock_acquire();

    for (uint32_t i = 0; i < g_vmap_len; i++) {
        struct vmap_entry *entry = &g_vmap_entries[i];
        if (entry->len < aligned_size) {
            continue;
        }

        virt_addr_t va_base = entry->addr;
        if (entry->len == aligned_size) {
            remove_entry(i);
        } else {
            entry->addr += aligned_size;
            entry->len -= aligned_size;
        }

        pagetable_map(mm_kptbl_get(), va_base, aligned_start, aligned_size, PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE);

        irqlock_release(&irqlock);

        *va_out = (void *)(va_base + (pa - aligned_start));
        return (struct span){
            .ptr = (void *)va_base,
            .size = aligned_size,
        };
    }

    irqlock_release(&irqlock);
    return (struct span){ .ptr = 0, .size = 0 };
}

void mm_vmap_free(struct span span) {
    if (!span.ptr || span.size == 0) {
        return;
    }

    const virt_addr_t addr = (virt_addr_t)span.ptr;
    const virt_size_t len = span.size;
    const virt_addr_t end = addr + len;

    assert(addr % PAGE_SIZE == 0);
    assert(len % PAGE_SIZE == 0);
    assert(VMAP_START_VIRT <= addr && addr < VMAP_END_VIRT);
    assert(len <= VMAP_END_VIRT - addr);

    irqlock_t irqlock = irqlock_acquire();

    pagetable_unmap(mm_kptbl_get(), addr, len, true);

    uint32_t idx = 0;
    while (idx < g_vmap_len && g_vmap_entries[idx].addr < addr) {
        idx++;
    }

    if (idx > 0) {
        const struct vmap_entry *prev = &g_vmap_entries[idx - 1];
        assert(prev->addr + prev->len <= addr, "double free or overlap detected");
    }
    if (idx < g_vmap_len) {
        const struct vmap_entry *next = &g_vmap_entries[idx];
        assert(end <= next->addr, "double free or overlap detected");
    }

    const bool merge_prev =
        idx > 0 && (g_vmap_entries[idx - 1].addr + g_vmap_entries[idx - 1].len == addr);
    const bool merge_next =
        idx < g_vmap_len && (end == g_vmap_entries[idx].addr);

    if (merge_prev && merge_next) {
        struct vmap_entry *prev = &g_vmap_entries[idx - 1];
        struct vmap_entry *next = &g_vmap_entries[idx];
        prev->len += len + next->len;
        remove_entry(idx);
        goto exit;
    }

    if (merge_prev) {
        struct vmap_entry *prev = &g_vmap_entries[idx - 1];
        prev->len += len;
        goto exit;
    }

    if (merge_next) {
        struct vmap_entry *next = &g_vmap_entries[idx];
        next->addr = addr;
        next->len += len;
        goto exit;
    }

    insert_entry(idx, addr, len);

exit:
    irqlock_release(&irqlock);
}
