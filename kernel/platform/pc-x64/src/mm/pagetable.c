#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <kc/assert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/mm/map.h>
#include <opal/platform/asm.h>
#include <opal/platform/mm/pagetable.h>

#define PAGETABLE_LENGTH 512

typedef page_entry_t pagetable_t[PAGETABLE_LENGTH];

// boot.asm
#ifdef TEST
static pagetable_t tmp_table;
#else
extern pagetable_t tmp_table;
#endif

static pagetable_t *g_pml4;

static virt_addr_t phys_to_virt_kernel(phys_addr_t pa) {
    return pa - KERNEL_START_PHYS + KERNEL_START_VIRT;
}

static pagetable_t *entry_to_table(page_entry_t entry) {
    return (pagetable_t *)phys_to_virt_kernel(entry & PTE_MASK_ADDR);
}

static phys_addr_t alloc_usable_page(void) {
    size_t allocated;
    return mm_usable_alloc_metadata(1, &allocated);
}

static pagetable_t *get_or_alloc_table(pagetable_t *parent, size_t index, page_entry_t flags, bool alloc) {
    const page_entry_t entry = (*parent)[index];

    if (entry & PTE_FLAG_PRESENT) {
        if (entry & PTE_FLAG_HUGE) {
            panic("unexpected large page while descending page tables");
        }
        return entry_to_table(entry);
    }

    if (!alloc) {
        panic("page table is not found");
    }

    const phys_addr_t child_pa = alloc_usable_page();
    pagetable_t *child = (pagetable_t *)(uintptr_t)phys_to_virt_kernel(child_pa);

    memset(child, 0, sizeof(pagetable_t));
    (*parent)[index] = (page_entry_t)(child_pa | flags | PTE_FLAG_PRESENT);
    return child;
}

void mm_pagetable_map(virt_addr_t va, phys_addr_t pa, uint64_t flags, bool alloc) {
    const size_t i4 = (size_t)((va >> 39) & 0x1ffu);
    const size_t i3 = (size_t)((va >> 30) & 0x1ffu);
    const size_t i2 = (size_t)((va >> 21) & 0x1ffu);
    const size_t i1 = (size_t)((va >> 12) & 0x1ffu);

    pagetable_t *const pdpt = get_or_alloc_table(g_pml4, i4, flags, alloc);
    pagetable_t *const pd = get_or_alloc_table(pdpt, i3, flags, alloc);
    pagetable_t *const pt = get_or_alloc_table(pd, i2, flags, alloc);
    page_entry_t *const pte = &(*pt)[i1];

    *pte = (page_entry_t)((pa & PTE_MASK_ADDR) | flags);
}

static void pagetable_init_prepare(void) {
    const phys_addr_t new_pml4_pa = alloc_usable_page();
    pagetable_t *new_pml4 = (pagetable_t *)(uintptr_t)phys_to_virt_kernel(new_pml4_pa);
    pagetable_t *old_pml4 = &tmp_table;

    memcpy(new_pml4, old_pml4, sizeof(pagetable_t));

    g_pml4 = new_pml4;

    write_cr3(new_pml4_pa);
}

static void pagetable_init(const struct mmap *snapshot) {
    virt_addr_t va = PAGETABLE_START_VIRT;

    for (size_t i = 0; i < snapshot->length; i++) {
        const struct mmap_entry* entry = &snapshot->entries[i];

        for (size_t page = 0; page < entry->len / PAGE_SIZE; page++) {
            mm_pagetable_map(va, entry->addr + page * PAGE_SIZE, PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE, true);
            tlb_flush_for(va);
            va += PAGE_SIZE;
        }
    }
}

void mm_pagetable_init(const struct mmap *snapshot) {
    pagetable_init_prepare();
    pagetable_init(snapshot);
}

#include <opal/tty.h>
#include <opal/mm/page.h>

static void print_pte_flags(page_entry_t entry) {
    bool printed = false;
#define FLAG(name) \
    if (entry & PTE_FLAG_##name) { \
        if (!printed) printed = true; \
        else tty0_printf(" | "); \
        tty0_printf(#name); \
    }
    FLAG(PRESENT)
    FLAG(WRITABLE)
    FLAG(USER)
    FLAG(WRITE_THROUGH)
    FLAG(NO_CACHE)
    FLAG(ACCESSED)
    FLAG(DIRTY)
    FLAG(HUGE)
    FLAG(GLOBAL)
    FLAG(NO_EXECUTE)
#undef FLAG
    if (!printed) {
        tty0_printf("0");
    }
}

static bool phys_to_virt(phys_addr_t pa, virt_addr_t *va_out) {
    if (KERNEL_START_PHYS <= pa && pa < (uintptr_t)__kernel_phys_end) {
        *va_out = phys_to_virt_kernel(pa);
        return true;
    } else {
        return phys_to_virt_metadata(pa, va_out);
    }
}

static void print_pagetable_recur(pagetable_t *table,
    const char *names[], unsigned depth, uintptr_t pagesize, uintptr_t va
) {
    int leaf_begin = -1;
    for (int idx = 0; idx <= PAGETABLE_LENGTH; idx++) {
        bool present = idx < PAGETABLE_LENGTH && ((*table)[idx] & PTE_FLAG_PRESENT);
        bool leaf = present && (depth == 3 || ((*table)[idx] & PTE_FLAG_HUGE));

        if (leaf_begin != -1) {
            phys_addr_t prev = (*table)[idx - 1] & PTE_MASK_ADDR;
            if (!leaf || prev + pagesize != ((*table)[idx] & PTE_MASK_ADDR)) {
                unsigned shifts[] = { 39, 30, 21, 12 };

                phys_addr_t pa = (*table)[leaf_begin] & PTE_MASK_ADDR;
                phys_size_t len = (phys_size_t)(idx - leaf_begin) * pagesize;
                virt_addr_t sign = 0xffff800000000000;
                virt_addr_t v_raw = (va << 9 | (uintptr_t)leaf_begin) << shifts[depth];
                virt_addr_t v_ext = v_raw & sign ? v_raw | sign : v_raw;

                tty0_printf("%s %#018"PRIvirt"-%#018"PRIvirt" to %#"PRIphys"-%#"PRIphys"\n",
                    names[depth], v_ext, v_ext + len, pa, pa + len);
                leaf_begin = leaf ? idx : -1;
            }
        }
        if (leaf) {
            if (leaf_begin == -1) {
                leaf_begin = idx;
            }
            continue;
        }
        if (!present) {
            continue;
        }

        page_entry_t entry = (*table)[idx];
        phys_addr_t pa = entry & PTE_MASK_ADDR;

        tty0_printf("%s %#7x to %#"PRIphys": ", names[depth], idx, pa);
        print_pte_flags(entry);
        tty0_printf("\n");

        virt_addr_t va_subtable;
        if (phys_to_virt(pa, &va_subtable)) {
            pagetable_t *subtable = (pagetable_t *)va_subtable;
            print_pagetable_recur(subtable, names, depth + 1, pagesize >> 9, va << 9 | idx);
        } else {
            tty0_printf("%s ????\n", names[depth + 1]);
        }
    }
}

void mm_pagetable_print(void) {
    const char *names[] = { "PML4E", " PDPE", "  PDE", "   PT" };
    print_pagetable_recur(g_pml4, names, 0, 0x0000008000000000, 0);
}
