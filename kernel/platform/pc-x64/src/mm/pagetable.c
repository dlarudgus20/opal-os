#include <stddef.h>
#include <stdint.h>

#include <kc/kassert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/mm/mm.h>
#include <opal/mm/pfn.h>
#include <opal/mm/page.h>
#include <opal/mm/map.h>
#include <opal/mm/tmpalloc.h>
#include <opal/platform/asm.h>
#include <opal/platform/mm/pagetable.h>
#include <opal/platform/boot/bootinfo.h>

#define PAGETABLE_LENGTH 512
#define BOOTSTRAP_MAP_END_PHYS 0x00a00000u

#define HUGE_PAGE_SIZE 0x200000

static struct pagetable *g_kptbl;

static bool g_direct_ready = false;
static struct tmpalloc *g_tmpalloc;

static virt_addr_t phys_to_virt_kernel(phys_addr_t pa) {
    return pa - KERNEL_START_PHYS + KERNEL_START_VIRT;
}

static virt_addr_t phys_to_virt_direct(phys_addr_t pa) {
    return DIRECT_MAP_START_VIRT + pa;
}

static phys_addr_t virt_to_phys_direct(virt_addr_t va) {
    return va - DIRECT_MAP_START_VIRT;
}

static virt_addr_t phys_to_virt_table(phys_addr_t pa) {
    if (!g_direct_ready) {
        kassert(pa < BOOTSTRAP_MAP_END_PHYS, "bootstrap pagetable overflow");
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
            return 0;
        }
        return pfn_to_phys(pfn);
    }
}

static bool deallocate_page(phys_addr_t pa) {
    kassert(!g_tmpalloc, "cannot deallocate page while using tmpalloc");

    pfn_t pfn = phys_to_pfn(pa);
    struct page *page = pfn_to_page(pfn);
    if (page->flags & PAGE_FLAG_METADATA) {
        return false;
    }

    mm_free_page(phys_to_pfn(pa), 0);
    return true;
}

static bool expand_hugepage(page_entry_t *pde) {
    const phys_addr_t pt_pa = allocate_page();
    if (pt_pa == 0) {
        return false;
    }

    struct pagetable *const pt = (struct pagetable *)phys_to_virt_table(pt_pa);
    const page_entry_t flags = (*pde & ~PTE_MASK_ADDR) & ~PTE_FLAG_HUGE;

    phys_addr_t pa = *pde & PTE_MASK_ADDR;
    for (unsigned i = 0; i < PAGETABLE_LENGTH; i++) {
        pt->entries[i] = pa | flags;
        pa += PAGE_SIZE;
    }

    *pde = pt_pa | flags;
    return true;
}

static struct pagetable *get_or_alloc_table(struct pagetable *parent, size_t index, page_entry_t flags, bool allow_huge) {
    page_entry_t *const entry = &parent->entries[index];

    if (*entry & PTE_FLAG_PRESENT) {
        if (*entry & PTE_FLAG_HUGE) {
            kassert(allow_huge, "unexpected huge page");
            if (!expand_hugepage(entry)) {
                return NULL;
            }
        }
        return (struct pagetable *)phys_to_virt_table(*entry & PTE_MASK_ADDR);
    }

    const phys_addr_t child_pa = allocate_page();
    if (child_pa == 0) {
        return NULL;
    }
    struct pagetable *child = (struct pagetable *)phys_to_virt_table(child_pa);

    memset(child, 0, sizeof(*child));
    parent->entries[index] = (page_entry_t)(child_pa | PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE | flags);
    return child;
}

static bool map_4k(struct pagetable *ptbl, virt_addr_t va, phys_addr_t pa, page_entry_t flags) {
    const size_t i4 = (va >> 39) & 0x1ffu;
    const size_t i3 = (va >> 30) & 0x1ffu;
    const size_t i2 = (va >> 21) & 0x1ffu;
    const size_t i1 = (va >> 12) & 0x1ffu;

    struct pagetable *const pdpt = get_or_alloc_table(ptbl, i4, flags, false);
    if (!pdpt) {
        return false;
    }
    struct pagetable *const pd = get_or_alloc_table(pdpt, i3, flags, false);
    if (!pd) {
        return false;
    }
    struct pagetable *const pt = get_or_alloc_table(pd, i2, flags, true);
    if (!pt) {
        return false;
    }

    page_entry_t *const pte = &pt->entries[i1];
    if (*pte & PTE_FLAG_PRESENT) {
        return false;
    }

    *pte = (page_entry_t)((pa & PTE_MASK_ADDR) | flags);
    return true;
}

static bool map_2m(struct pagetable *ptbl, virt_addr_t va, phys_addr_t pa, page_entry_t flags) {
    const size_t i4 = (va >> 39) & 0x1ffu;
    const size_t i3 = (va >> 30) & 0x1ffu;
    const size_t i2 = (va >> 21) & 0x1ffu;

    struct pagetable *const pdpt = get_or_alloc_table(ptbl, i4, flags, false);
    if (!pdpt) {
        return false;
    }
    struct pagetable *const pd = get_or_alloc_table(pdpt, i3, flags, false);
    if (!pd) {
        return false;
    }

    page_entry_t *const pde = &pd->entries[i2];
    if (*pde & PTE_FLAG_PRESENT) {
        return false;
    }

    *pde = (page_entry_t)((pa & PTE_MASK_ADDR) | flags | PTE_FLAG_HUGE);
    return true;
}

static virt_addr_t map_range_len(struct pagetable *ptbl, virt_addr_t va, phys_addr_t pa, phys_size_t len, page_entry_t flags) {
    kassert(pa % PAGE_SIZE == 0);
    kassert(len % PAGE_SIZE == 0);
    kassert(va % PAGE_SIZE == 0);

    if (va == 0) {
        return 0;
    }

    virt_addr_t va_cur = va;
    phys_addr_t pa_cur = pa;

    while (len > 0) {
        bool can_use_huge =
            (len >= HUGE_PAGE_SIZE) &&
            ((va_cur & (HUGE_PAGE_SIZE - 1)) == 0) &&
            ((pa_cur & (HUGE_PAGE_SIZE - 1)) == 0);

        if (can_use_huge) {
            if (!map_2m(ptbl, va_cur, pa_cur, flags)) {
                goto err;
            }
            va_cur += HUGE_PAGE_SIZE;
            pa_cur += HUGE_PAGE_SIZE;
            len -= HUGE_PAGE_SIZE;
        } else {
            if (!map_4k(ptbl, va_cur, pa_cur, flags)) {
                goto err;
            }
            va_cur += PAGE_SIZE;
            pa_cur += PAGE_SIZE;
            len -= PAGE_SIZE;
        }
    }

    return va_cur;

err:
    pagetable_unmap(ptbl, va, pa_cur - pa, false);
    return 0;
}

static virt_addr_t map_range(struct pagetable *ptbl, virt_addr_t va, phys_addr_t pa_start, phys_addr_t pa_end, page_entry_t flags) {
    return map_range_len(ptbl, va, pa_start, pa_end - pa_start, flags);
}

static bool deallocate_pt(phys_addr_t pt_pa) {
    if (deallocate_page(pt_pa)) {
        return true;
    }
    struct pagetable *pt = (struct pagetable *)phys_to_virt_table(pt_pa);
    memset(pt, 0, sizeof(*pt));
    return false;
}

static void unmap_table(struct pagetable *pd, bool (*deallocate)(phys_addr_t)) {
    for (size_t i = 0; i < PAGETABLE_LENGTH; i++) {
        page_entry_t *const entry = &pd->entries[i];
        if (!(*entry & PTE_FLAG_PRESENT)) {
            continue;
        }

        const phys_addr_t pt_pa = *entry & PTE_MASK_ADDR;
        if ((*entry & PTE_FLAG_HUGE) || deallocate(pt_pa)) {
            *entry = 0;
        }
    }
}

static bool deallocate_pdt(phys_addr_t pd_pa) {
    struct pagetable *pd = (struct pagetable *)phys_to_virt_table(pd_pa);
    unmap_table(pd, deallocate_pt);
    return deallocate_page(pd_pa);
}

static bool deallocate_pdpt(phys_addr_t pdpt_pa) {
    struct pagetable *pdpt = (struct pagetable *)phys_to_virt_table(pdpt_pa);
    unmap_table(pdpt, deallocate_pdt);
    return deallocate_page(pdpt_pa);
}

static virt_addr_t unmap_next(struct pagetable *ptbl, virt_addr_t va, virt_addr_t end) {
    const uint16_t i4 = (va >> 39) & 0x1ffu;
    const uint16_t i3 = (va >> 30) & 0x1ffu;
    const uint16_t i2 = (va >> 21) & 0x1ffu;
    const uint16_t i1 = (va >> 12) & 0x1ffu;

    const virt_addr_t bits4 = 1ull << 39;
    const virt_addr_t bits3 = 1ull << 30;
    const virt_addr_t bits2 = 1ull << 21;
    const virt_addr_t bits1 = 1ull << 12;

    page_entry_t *const pml4e = &ptbl->entries[i4];
    kassert(!(*pml4e & PTE_FLAG_HUGE), "unexpected huge page");
    if (va + bits4 <= end && (*pml4e & PTE_FLAG_PRESENT) && deallocate_pdpt(*pml4e & PTE_MASK_ADDR)) {
        *pml4e = 0;
    }
    if (!(*pml4e & PTE_FLAG_PRESENT)) {
        return (va + bits4) & ~(bits4 - 1);
    }

    struct pagetable *const pdpt = (struct pagetable *)phys_to_virt_table(*pml4e & PTE_MASK_ADDR);
    page_entry_t *const pdpe = &pdpt->entries[i3];
    kassert(!(*pdpe & PTE_FLAG_HUGE), "unexpected huge page");
    if (va + bits3 <= end && (*pdpe & PTE_FLAG_PRESENT) && deallocate_pdt(*pdpe & PTE_MASK_ADDR)) {
        *pdpe = 0;
    }
    if (!(*pdpe & PTE_FLAG_PRESENT)) {
        return (va + bits3) & ~(bits3 - 1);
    }

    struct pagetable *const pd = (struct pagetable *)phys_to_virt_table(*pdpe & PTE_MASK_ADDR);
    page_entry_t *const pde = &pd->entries[i2];
    if (va + bits2 <= end && (*pde & PTE_FLAG_PRESENT)) {
        if ((*pde & PTE_FLAG_HUGE) || deallocate_pt(*pde & PTE_MASK_ADDR)) {
            *pde = 0;
        }
    }
    if (!(*pde & PTE_FLAG_PRESENT)) {
        return (va + bits2) & ~(bits2 - 1);
    }
    if (*pde & PTE_FLAG_HUGE && !expand_hugepage(pde)) {
        return va;
    }

    struct pagetable *const pt = (struct pagetable *)phys_to_virt_table(*pde & PTE_MASK_ADDR);
    pt->entries[i1] = 0;
    return va + bits1;
}

static virt_addr_t unmap_range_len(struct pagetable *ptbl, virt_addr_t va, virt_size_t len, bool flush_tlb) {
    kassert(va % PAGE_SIZE == 0);
    kassert(len % PAGE_SIZE == 0);

    const virt_addr_t end = va + len;
    const bool invlpg = flush_tlb && (len < HUGE_PAGE_SIZE);

    while (va != end) {
        const virt_addr_t va_next = unmap_next(ptbl, va, end);
        if (va_next == va) {
            break;
        }
        if (invlpg) {
            tlb_flush_for(va);
        }
        va = (end == 0 || va_next < end) ? va_next : end;
    }

    if (flush_tlb && !invlpg) {
        write_cr3(read_cr3());
    }
    return va;
}

struct pagetable *pagetable_clone(struct pagetable *ptbl) {
    struct pagetable *clone = mm_alloc_page_ptr(0);
    if (!clone) {
        return NULL;
    }
    *clone = *ptbl;
    return clone;
}

struct pagetable *pagetable_create(void) {
    return pagetable_clone(g_kptbl);
}

void pagetable_destroy(struct pagetable *ptbl) {
    pagetable_unmap(ptbl, 0, 0x0000800000000000, false);
    mm_free_page_ptr(ptbl, 0);
}

void pagetable_apply(struct pagetable *ptbl) {
    write_cr3(virt_to_phys_direct((virt_addr_t)ptbl));
}

virt_addr_t pagetable_map(struct pagetable *ptbl, virt_addr_t va, phys_addr_t pa, phys_size_t len, page_entry_t flags) {
    return map_range_len(ptbl, va, pa, len, flags);
}

virt_addr_t pagetable_unmap(struct pagetable *ptbl, virt_addr_t va, virt_size_t len, bool flush_tlb) {
    return unmap_range_len(ptbl, va, len, flush_tlb);
}

bool pagetable_lookup(struct pagetable *ptbl, virt_addr_t va, phys_addr_t *pa_out) {
    if (!ptbl || !pa_out) {
        return false;
    }

    const size_t i4 = (va >> 39) & 0x1ffu;
    const size_t i3 = (va >> 30) & 0x1ffu;
    const size_t i2 = (va >> 21) & 0x1ffu;
    const size_t i1 = (va >> 12) & 0x1ffu;

    const page_entry_t pml4e = ptbl->entries[i4];
    if (!(pml4e & PTE_FLAG_PRESENT) || (pml4e & PTE_FLAG_HUGE)) {
        return false;
    }

    struct pagetable *const pdpt = (struct pagetable *)phys_to_virt_table(pml4e & PTE_MASK_ADDR);
    const page_entry_t pdpe = pdpt->entries[i3];
    if (!(pdpe & PTE_FLAG_PRESENT)) {
        return false;
    }
    if (pdpe & PTE_FLAG_HUGE) {
        *pa_out = (pdpe & PTE_MASK_ADDR) + (va & ((1ull << 30) - 1));
        return true;
    }

    struct pagetable *const pd = (struct pagetable *)phys_to_virt_table(pdpe & PTE_MASK_ADDR);
    const page_entry_t pde = pd->entries[i2];
    if (!(pde & PTE_FLAG_PRESENT)) {
        return false;
    }
    if (pde & PTE_FLAG_HUGE) {
        *pa_out = (pde & PTE_MASK_ADDR) + (va & ((1ull << 21) - 1));
        return true;
    }

    struct pagetable *const pt = (struct pagetable *)phys_to_virt_table(pde & PTE_MASK_ADDR);
    const page_entry_t pte = pt->entries[i1];
    if (!(pte & PTE_FLAG_PRESENT)) {
        return false;
    }

    *pa_out = (pte & PTE_MASK_ADDR) + (va & (PAGE_SIZE - 1));
    return true;
}

void mm_kptbl_init(struct tmpalloc *ta) {
    g_tmpalloc = ta;

    const phys_addr_t new_pml4_pa = allocate_page();
    struct pagetable *ptbl = (struct pagetable *)phys_to_virt_kernel(new_pml4_pa);
    memset(ptbl, 0, sizeof(*ptbl));

    // fill kernel area pdpt
    for (unsigned i = 0; i < PAGETABLE_LENGTH / 2; i++) {
        const phys_addr_t pdpt_pa = allocate_page();
        struct pagetable *pdpt = (struct pagetable *)phys_to_virt_kernel(pdpt_pa);
        memset(pdpt, 0, sizeof(*pdpt));
        ptbl->entries[PAGETABLE_LENGTH / 2 + i] = pdpt_pa | PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE;
    }

    // kernel image
    virt_addr_t va = KERNEL_START_VIRT;
    va = map_range(ptbl, va, (phys_addr_t)__kernel_start_lba, (phys_addr_t)__rodata_end_lba, PTE_FLAG_PRESENT);
    va = map_range(ptbl, va, (phys_addr_t)__rodata_end_lba, (phys_addr_t)__before_stack_lba, PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE);
    map_range(ptbl, KSTACK_START_VIRT, (phys_addr_t)__stack_bottom_lba, (phys_addr_t)__kernel_end_lba, PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE);

    // direct map (bootstrap)
    map_range(ptbl, DIRECT_MAP_START_VIRT, 0, BOOTSTRAP_MAP_END_PHYS, PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE);
    g_direct_ready = true;

    // update cr3
    // old ptbl points in bootstrap map, which is invalid from now.
    write_cr3(new_pml4_pa);
    ptbl = (struct pagetable *)phys_to_virt_direct(new_pml4_pa);
    g_kptbl = ptbl;

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

        map_range_len(ptbl, va_start, addr, len, PTE_FLAG_PRESENT | PTE_FLAG_WRITABLE);
    }
}

void mm_kptbl_unuse_tmpalloc(void) {
    g_tmpalloc = NULL;
}

struct pagetable *mm_kptbl_get(void) {
    return g_kptbl;
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

static void print_pagetable_recur(struct pagetable *table,
    const char *names[], unsigned depth, uintptr_t pagesize, uintptr_t va
) {
    int leaf_begin = -1;
    for (int idx = 0; idx <= PAGETABLE_LENGTH; idx++) {
        bool present = idx < PAGETABLE_LENGTH && (table->entries[idx] & PTE_FLAG_PRESENT);
        bool leaf = present && (depth == 3 || (table->entries[idx] & PTE_FLAG_HUGE));

        if (leaf_begin != -1) {
            const page_entry_t prev = table->entries[idx - 1];
            const page_entry_t prev_flags = get_leaf_flags(prev);
            const phys_addr_t prev_addr = prev & PTE_MASK_ADDR;

            bool skip = leaf;
            if (leaf) {
                skip &= prev_addr + pagesize == (table->entries[idx] & PTE_MASK_ADDR);
                skip &= prev_flags == get_leaf_flags(table->entries[idx]);
            }

            if (!skip) {
                unsigned shifts[] = { 39, 30, 21, 12 };

                phys_addr_t pa = table->entries[leaf_begin] & PTE_MASK_ADDR;
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

        page_entry_t entry = table->entries[idx];
        phys_addr_t pa = entry & PTE_MASK_ADDR;

        tty0_printf("%s %#7x to %#"PRIphys": ", names[depth], idx, pa);
        print_pte_flags(entry);
        tty0_printf("\n");

        struct pagetable *subtable = (struct pagetable *)phys_to_virt_direct(pa);
        print_pagetable_recur(subtable, names, depth + 1, pagesize >> 9, va << 9 | idx);
    }
}

void mm_kptbl_print(void) {
    const char *names[] = { "PML4E", " PDPE", "  PDE", "   PT" };
    print_pagetable_recur(g_kptbl, names, 0, (uintptr_t)1 << 39, 0);
}
