#include <stddef.h>
#include <stdint.h>

#include <kc/assert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/mm/mm.h>
#include <opal/mm/pfn.h>
#include <opal/mm/map.h>
#include <opal/mm/tmpalloc.h>
#include <opal/platform/asm.h>
#include <opal/platform/mm/pagetable.h>
#include <opal/platform/boot/bootinfo.h>

#define PAGETABLE_LENGTH 512
#define BOOTSTRAP_MAP_END_PHYS 0x00a00000u

#define HUGE_PAGE_SIZE 0x200000

typedef page_entry_t pagetable_t[PAGETABLE_LENGTH];

static pagetable_t *g_ptable;
static bool g_direct_access_ready = false;

static struct tmpalloc *g_tmpalloc;

static virt_addr_t phys_to_virt_kernel(phys_addr_t pa) {
    return pa - KERNEL_START_PHYS + KERNEL_START_VIRT;
}

static virt_addr_t phys_to_virt_direct(phys_addr_t pa) {
    return DIRECT_MAP_START_VIRT + pa;
}

static virt_addr_t phys_to_virt_table(phys_addr_t pa) {
    if (!g_direct_access_ready) {
        assert(pa < BOOTSTRAP_MAP_END_PHYS, "bootstrap pagetable overflow");
        return phys_to_virt_kernel(pa);
    } else {
        return phys_to_virt_direct(pa);
    }
}

static phys_addr_t allocate_page(void) {
    if (g_tmpalloc) {
        size_t allocated;
        return tmpalloc_alloc_pages(g_tmpalloc, 1, &allocated);
    } else {
        pfn_t pfn = mm_alloc_page(0);
        if (pfn == PFN_INVALID) {
            panic("cannot allocate more page table");
        }
        return mm_pfn_to_phys(pfn);
    }
}

static pagetable_t *get_or_alloc_table(pagetable_t *parent, size_t index) {
    const page_entry_t entry = (*parent)[index];

    if (entry & PTE_FLAG_PRESENT) {
        if (entry & PTE_FLAG_HUGE) {
            panic("unexpected large page while descending page tables");
        }
        return (pagetable_t *)phys_to_virt_table(entry & PTE_MASK_ADDR);
    }

    const phys_addr_t child_pa = allocate_page();
    pagetable_t *child = (pagetable_t *)phys_to_virt_table(child_pa);

    memset(child, 0, sizeof(pagetable_t));
    (*parent)[index] = (page_entry_t)(child_pa | PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE);
    return child;
}

static void map_4k(virt_addr_t va, phys_addr_t pa, page_entry_t flags) {
    const size_t i4 = (size_t)((va >> 39) & 0x1ffu);
    const size_t i3 = (size_t)((va >> 30) & 0x1ffu);
    const size_t i2 = (size_t)((va >> 21) & 0x1ffu);
    const size_t i1 = (size_t)((va >> 12) & 0x1ffu);

    pagetable_t *const pdpt = get_or_alloc_table(g_ptable, i4);
    pagetable_t *const pd = get_or_alloc_table(pdpt, i3);
    pagetable_t *const pt = get_or_alloc_table(pd, i2);
    page_entry_t *const pte = &(*pt)[i1];

    *pte = (page_entry_t)((pa & PTE_MASK_ADDR) | flags);
}

static void map_2m(virt_addr_t va, phys_addr_t pa, page_entry_t flags) {
    const size_t i4 = (size_t)((va >> 39) & 0x1ffu);
    const size_t i3 = (size_t)((va >> 30) & 0x1ffu);
    const size_t i2 = (size_t)((va >> 21) & 0x1ffu);

    pagetable_t *const pdpt = get_or_alloc_table(g_ptable, i4);
    pagetable_t *const pd = get_or_alloc_table(pdpt, i3);
    page_entry_t *const pde = &(*pd)[i2];

    if ((*pde & PTE_FLAG_PRESENT) && !(*pde & PTE_FLAG_HUGE)) {
        pagetable_t *const pt = (pagetable_t *)phys_to_virt_table(*pde & PTE_MASK_ADDR);
        for (phys_size_t i = 0; i < PAGETABLE_LENGTH; i++) {
            const phys_addr_t pa_i = pa + i * PAGE_SIZE;
            (*pt)[i] = (page_entry_t)((pa_i & PTE_MASK_ADDR) | flags);
        }
    }

    *pde = (page_entry_t)((pa & PTE_MASK_ADDR) | flags | PTE_FLAG_HUGE);
}

static virt_addr_t map_range_len(virt_addr_t va, phys_addr_t pa, phys_size_t len, page_entry_t flags) {
    assert(pa % PAGE_SIZE == 0);
    assert(len % PAGE_SIZE == 0);
    assert(va % PAGE_SIZE == 0);

    virt_addr_t va_cur = va;
    phys_addr_t pa_cur = pa;

    while (len > 0) {
        bool can_use_huge =
            (len >= HUGE_PAGE_SIZE) &&
            ((va_cur & (HUGE_PAGE_SIZE - 1)) == 0) &&
            ((pa_cur & (HUGE_PAGE_SIZE - 1)) == 0);

        if (can_use_huge) {
            map_2m(va_cur, pa_cur, flags);
            va_cur += HUGE_PAGE_SIZE;
            pa_cur += HUGE_PAGE_SIZE;
            len -= HUGE_PAGE_SIZE;
        } else {
            map_4k(va_cur, pa_cur, flags);
            va_cur += PAGE_SIZE;
            pa_cur += PAGE_SIZE;
            len -= PAGE_SIZE;
        }
    }

    return va_cur;
}

static virt_addr_t map_range(virt_addr_t va, phys_addr_t pa_start, phys_addr_t pa_end, page_entry_t flags) {
    return map_range_len(va, pa_start, pa_end - pa_start, flags);
}

virt_addr_t mm_pagetable_map(virt_addr_t va, phys_addr_t pa, phys_size_t len, page_entry_t flags) {
    return map_range_len(va, pa, len, flags);
}

void mm_pagetable_unmap(virt_addr_t va, phys_size_t len) {
    assert(va % PAGE_SIZE == 0);
    assert(len % PAGE_SIZE == 0);

    if (len == 0) {
        return;
    }

    // TODO: implement page table walk and clear entries.
    // TODO: flush TLB for unmapped range.
    (void)va;
    (void)len;

    panic("mm_pagetable_unmap(): unimplemneted");
}

void mm_pagetable_init(struct tmpalloc *ta) {
    g_tmpalloc = ta;

    const phys_addr_t new_pml4_pa = allocate_page();
    g_ptable = (pagetable_t *)phys_to_virt_kernel(new_pml4_pa);
    memset(g_ptable, 0, sizeof(*g_ptable));

    // kernel image
    virt_addr_t va = KERNEL_START_VIRT;
    va = map_range(va, (phys_addr_t)__kernel_start_lba, (phys_addr_t)__rodata_end_lba, PTE_FLAG_PRESENT);
    va = map_range(va, (phys_addr_t)__rodata_end_lba, (phys_addr_t)__before_stack_lba, PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE);
    map_range(KSTACK_START_VIRT, (phys_addr_t)__stack_bottom_lba, (phys_addr_t)__kernel_end_lba, PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE);

    // direct map (bootstrap)
    map_range(DIRECT_MAP_START_VIRT, 0, BOOTSTRAP_MAP_END_PHYS, PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE);
    g_direct_access_ready = true;

    // update cr3
    // old g_ptable points in bootstrap map, which is invalid from now.
    write_cr3(new_pml4_pa);
    g_ptable = (pagetable_t *)phys_to_virt_direct(new_pml4_pa);

    // remaining direct map
    const struct mmap *sec = mm_get_section_map();
    for (size_t i = 0; i < sec->length; i++) {
        const struct mmap_entry* entry = &sec->entries[i];

        phys_addr_t addr = entry->addr;
        phys_size_t len = entry->len;

        if (entry->addr + entry->len - 1 < BOOTSTRAP_MAP_END_PHYS) {
            continue;
        }

        if (entry->addr < BOOTSTRAP_MAP_END_PHYS) {
            addr = BOOTSTRAP_MAP_END_PHYS;
            len -= BOOTSTRAP_MAP_END_PHYS - entry->addr;
        }

        const virt_addr_t va_start = DIRECT_MAP_START_VIRT + addr;
        virt_addr_t va_end = va_start + len;

        if (va_start >= DIRECT_MAP_END_VIRT) {
            // too much lowmem
            break;
        }

        if (va_end < va_start || va_end > DIRECT_MAP_END_VIRT) {
            len = DIRECT_MAP_END_VIRT - va_start;
        }

        map_range_len(va_start, addr, len, PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE);
    }
}

void mm_pagetable_unuse_tmpalloc(void) {
    g_tmpalloc = NULL;
}

#include <opal/tty.h>

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

static page_entry_t get_leaf_flags(page_entry_t entry) {
    return (entry & ~PTE_MASK_ADDR) & ~(PTE_FLAG_ACCESSED | PTE_FLAG_DIRTY);
}

static void print_pagetable_recur(pagetable_t *table,
    const char *names[], unsigned depth, uintptr_t pagesize, uintptr_t va
) {
    int leaf_begin = -1;
    for (int idx = 0; idx <= PAGETABLE_LENGTH; idx++) {
        bool present = idx < PAGETABLE_LENGTH && ((*table)[idx] & PTE_FLAG_PRESENT);
        bool leaf = present && (depth == 3 || ((*table)[idx] & PTE_FLAG_HUGE));

        if (leaf_begin != -1) {
            const page_entry_t prev = (*table)[idx - 1];
            const page_entry_t prev_flags = get_leaf_flags(prev);
            const phys_addr_t prev_addr = prev & PTE_MASK_ADDR;

            bool skip = leaf;
            if (leaf) {
                skip &= prev_addr + pagesize == ((*table)[idx] & PTE_MASK_ADDR);
                skip &= prev_flags == get_leaf_flags((*table)[idx]);
            }

            if (!skip) {
                unsigned shifts[] = { 39, 30, 21, 12 };

                phys_addr_t pa = (*table)[leaf_begin] & PTE_MASK_ADDR;
                phys_size_t len = (phys_size_t)(idx - leaf_begin) * pagesize;
                virt_addr_t sign = 0xffff800000000000;
                virt_addr_t v_raw = (va << 9 | (virt_addr_t)leaf_begin) << shifts[depth];
                virt_addr_t v_ext = v_raw & sign ? v_raw | sign : v_raw;

                tty0_printf("%s %#018"PRIvirt"-%#018"PRIvirt" to %#"PRIphys"-%#"PRIphys": ",
                    names[depth], v_ext, v_ext + len, pa, pa + len);
                print_pte_flags(prev_flags);
                tty0_printf("\n");
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

        pagetable_t *subtable = (pagetable_t *)phys_to_virt_direct(pa);
        print_pagetable_recur(subtable, names, depth + 1, pagesize >> 9, va << 9 | idx);
    }
}

void mm_pagetable_print(void) {
    const char *names[] = { "PML4E", " PDPE", "  PDE", "   PT" };
    print_pagetable_recur(g_ptable, names, 0, (uintptr_t)1 << 39, 0);
}
