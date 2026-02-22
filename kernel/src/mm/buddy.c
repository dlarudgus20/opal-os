#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kc/assert.h>
#include <kc/stdlib.h>

#include <opal/mm/buddy.h>
#include <opal/mm/map.h>
#include <opal/mm/pfn.h>
#include <opal/platform/mm/defines.h>

#define BUDDY_MAX_ORDERS PFN_VALID_BIT_WIDTH

static struct linkedlist g_free_list[BUDDY_MAX_ORDERS];
static uint8_t g_max_order;
static size_t g_free_pages;

static pfn_t order_pages(uint8_t order) {
    assert(order < BUDDY_MAX_ORDERS);
    return (pfn_t)1 << order;
}

static bool list_is_empty(uint8_t order) {
    return linkedlist_is_empty(&g_free_list[order]);
}

static bool page_is_usable(pfn_t pfn) {
    if (!mm_pfn_is_valid(pfn)) {
        return false;
    }
    struct page *page = mm_pfn_to_page(pfn);
    return (page->flags & PAGE_FLAG_METADATA) == 0;
}

static void set_block_free_state(pfn_t pfn, uint8_t order, bool is_free) {
    const pfn_t pages = order_pages(order);
    for (pfn_t i = 0; i < pages; i++) {
        struct page *page = mm_pfn_to_page(pfn + i);
        if (is_free) {
            page->flags |= PAGE_FLAG_BUDDY_FREE;
        } else {
            page->flags &= ~PAGE_FLAG_BUDDY_FREE;
        }
        page->flags &= ~PAGE_FLAG_BUDDY_HEAD;
    }
}

static void list_push(uint8_t order, pfn_t pfn) {
    struct page *page = mm_pfn_to_page(pfn);
    set_block_free_state(pfn, order, true);
    page->flags |= PAGE_FLAG_BUDDY_FREE | PAGE_FLAG_BUDDY_HEAD;
    page->buddy.order = order;
    linkedlist_push_front(&g_free_list[order], &page->buddy.link);
}

static pfn_t list_pop(uint8_t order) {
    struct linkedlist_link *head_link = linkedlist_pop_front(&g_free_list[order]);
    assert(head_link != NULL);

    struct page *page = container_of(head_link, struct page, buddy.link);
    const pfn_t head = mm_page_to_pfn(page);
    set_block_free_state(head, order, false);
    page->buddy.link = (struct linkedlist_link){ .prev = NULL, .next = NULL };
    page->flags &= ~PAGE_FLAG_BUDDY_HEAD;
    page->buddy.order = 0;
    return head;
}

static bool is_free_head_of_order(pfn_t pfn, uint8_t order) {
    if (!mm_pfn_is_valid(pfn)) {
        return false;
    }

    struct page *page = mm_pfn_to_page(pfn);
    return (page->flags & PAGE_FLAG_BUDDY_FREE) &&
        (page->flags & PAGE_FLAG_BUDDY_HEAD) &&
        page->buddy.order == order;
}

static void list_remove(uint8_t order, pfn_t pfn) {
    assert(is_free_head_of_order(pfn, order));

    struct page *page = mm_pfn_to_page(pfn);
    linkedlist_remove(&page->buddy.link);
    set_block_free_state(pfn, order, false);
    page->buddy.link = (struct linkedlist_link){ .prev = NULL, .next = NULL };
    page->flags &= ~PAGE_FLAG_BUDDY_HEAD;
    page->buddy.order = 0;
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

        const pfn_t start = entry->addr / PAGE_SIZE;
        const pfn_t end = start + entry->len / PAGE_SIZE;
        add_range(start, end);
    }
}

void mm_buddy_init(void) {
    for (uint8_t i = 0; i < BUDDY_MAX_ORDERS; i++) {
        linkedlist_init(&g_free_list[i]);
    }
    g_free_pages = 0;

    const pfn_t pfn_end = mm_get_pfn_end();
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

    const pfn_t pfn = list_pop(cur);
    while (cur > order) {
        cur--;
        const pfn_t buddy = pfn + order_pages(cur);
        list_push(cur, buddy);
    }

    assert(g_free_pages >= order_pages(order));
    g_free_pages -= order_pages(order);
    return pfn;
}

void mm_buddy_free(pfn_t pfn, uint8_t order) {
    assert(order <= g_max_order, "invalid buddy order");
    assert(page_is_usable(pfn), "pfn is not allocated before");
    assert((pfn & (order_pages(order) - 1)) == 0, "invalid pfn with requested order");
    assert((mm_pfn_to_page(pfn)->flags & PAGE_FLAG_BUDDY_FREE) == 0, "pfn is not allocated before");

    const uint8_t req_order = order;

    while (order < g_max_order) {
        const pfn_t buddy = pfn ^ order_pages(order);
        if (buddy >= mm_get_pfn_end() || !is_free_head_of_order(buddy, order)) {
            break;
        }

        list_remove(order, buddy);

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
