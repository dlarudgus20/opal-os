#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kc/assert.h>

#include <opal/mm/buddy.h>
#include <opal/mm/map.h>
#include <opal/mm/pfn.h>
#include <opal/platform/mm/defines.h>

// TODO: calculate max_orders from log2( 2^64 / PAGE_SIZE )
enum {
    BUDDY_MAX_ORDERS = sizeof(pfn_t) * 8,
};

static pfn_t g_free_head[BUDDY_MAX_ORDERS];
static uint8_t g_max_order;
static size_t g_free_pages;

static pfn_t order_pages(uint8_t order) {
    assert(order < BUDDY_MAX_ORDERS);
    return (pfn_t)1 << order;
}

static bool list_is_empty(uint8_t order) {
    return g_free_head[order] == PFN_INVALID;
}

static bool page_is_usable(pfn_t pfn) {
    if (!mm_pfn_is_valid(pfn)) {
        return false;
    }
    struct page *page = mm_page_by_pfn(pfn);
    return (page->flags & PAGE_FLAG_METADATA) == 0;
}

static void set_block_free_state(pfn_t pfn, uint8_t order, bool is_free) {
    pfn_t pages = order_pages(order);
    for (pfn_t i = 0; i < pages; i++) {
        struct page *page = mm_page_by_pfn(pfn + i);
        if (is_free) {
            page->flags |= PAGE_FLAG_BUDDY_FREE;
        } else {
            page->flags &= ~PAGE_FLAG_BUDDY_FREE;
        }
        page->flags &= ~PAGE_FLAG_BUDDY_HEAD;
    }
}

static void list_push(uint8_t order, pfn_t pfn) {
    struct page *page = mm_page_by_pfn(pfn);
    set_block_free_state(pfn, order, true);
    page->flags |= PAGE_FLAG_BUDDY_FREE | PAGE_FLAG_BUDDY_HEAD;
    page->buddy_order = order;
    page->buddy_next = g_free_head[order];
    g_free_head[order] = pfn;
}

static pfn_t list_pop(uint8_t order) {
    pfn_t head = g_free_head[order];
    assert(head != PFN_INVALID);

    struct page *page = mm_page_by_pfn(head);
    g_free_head[order] = page->buddy_next;
    set_block_free_state(head, order, false);
    page->buddy_next = PFN_INVALID;
    page->flags &= ~PAGE_FLAG_BUDDY_HEAD;
    page->buddy_order = 0;
    return head;
}

static bool list_remove(uint8_t order, pfn_t pfn) {
    pfn_t *cur = &g_free_head[order];

    while (*cur != PFN_INVALID) {
        pfn_t cur_pfn = *cur;
        struct page *cur_page = mm_page_by_pfn(cur_pfn);

        if (cur_pfn == pfn) {
            *cur = cur_page->buddy_next;
            set_block_free_state(cur_pfn, order, false);
            cur_page->buddy_next = PFN_INVALID;
            cur_page->flags &= ~PAGE_FLAG_BUDDY_HEAD;
            cur_page->buddy_order = 0;
            return true;
        }

        cur = &cur_page->buddy_next;
    }

    return false;
}

static bool is_free_head_of_order(pfn_t pfn, uint8_t order) {
    if (!mm_pfn_is_valid(pfn)) {
        return false;
    }

    struct page *page = mm_page_by_pfn(pfn);
    return (page->flags & PAGE_FLAG_BUDDY_FREE) &&
        (page->flags & PAGE_FLAG_BUDDY_HEAD) &&
        page->buddy_order == order;
}

static void add_range(pfn_t start, pfn_t end) {
    pfn_t pfn = start;

    while (pfn < end) {
        pfn_t remaining = end - pfn;
        uint8_t order = 0;

        while (order + 1 <= g_max_order) {
            pfn_t block = order_pages(order + 1);
            if ((pfn & (block - 1)) != 0 || block > remaining) {
                break;
            }
            order++;
        }

        list_push(order, pfn);
        g_free_pages += order_pages(order);
        pfn += order_pages(order);
    }
}

static void prepare_from_section_map(void) {
    const struct mmap *sec = mm_get_section_map();
    for (uint32_t i = 0; i < sec->length; i++) {
        const struct mmap_entry *entry = &sec->entries[i];
        if (entry->type != MM_SEC_ENTRY_USABLE) {
            continue;
        }

        pfn_t start = entry->addr / PAGE_SIZE;
        pfn_t end = start + (pfn_t)(entry->len / PAGE_SIZE);
        add_range(start, end);
    }
}

void mm_buddy_init(void) {
    for (uint8_t i = 0; i < BUDDY_MAX_ORDERS; i++) {
        g_free_head[i] = PFN_INVALID;
    }
    g_free_pages = 0;

    pfn_t pfn_end = mm_get_pfn_end();
    g_max_order = 0;
    while (g_max_order + 1 < BUDDY_MAX_ORDERS && order_pages(g_max_order + 1) <= pfn_end) {
        g_max_order++;
    }

    prepare_from_section_map();
}

pfn_t mm_buddy_alloc(uint8_t order) {
    if (order > g_max_order) {
        return PFN_INVALID;
    }

    uint8_t cur = order;
    while (cur <= g_max_order && list_is_empty(cur)) {
        cur++;
    }
    if (cur > g_max_order) {
        return PFN_INVALID;
    }

    pfn_t pfn = list_pop(cur);
    while (cur > order) {
        cur--;
        pfn_t buddy = pfn + order_pages(cur);
        list_push(cur, buddy);
    }

    assert(g_free_pages >= order_pages(order));
    g_free_pages -= order_pages(order);
    return pfn;
}

void mm_buddy_free(pfn_t pfn, uint8_t order) {
    uint8_t req_order = order;
    assert(order <= g_max_order);
    assert(page_is_usable(pfn));
    assert((pfn & (order_pages(order) - 1)) == 0);
    assert((mm_page_by_pfn(pfn)->flags & PAGE_FLAG_BUDDY_FREE) == 0);

    while (order < g_max_order) {
        pfn_t buddy = pfn ^ order_pages(order);
        if (buddy >= mm_get_pfn_end() || !is_free_head_of_order(buddy, order)) {
            break;
        }

        bool removed = list_remove(order, buddy);
        assert(removed);

        if (buddy < pfn) {
            pfn = buddy;
        }
        order++;
    }

    list_push(order, pfn);
    g_free_pages += order_pages(req_order);
}

size_t mm_buddy_get_free_pages(void) {
    return g_free_pages;
}

uint8_t mm_buddy_get_max_order(void) {
    return g_max_order;
}
