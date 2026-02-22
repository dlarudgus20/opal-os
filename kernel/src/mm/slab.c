#include <stdalign.h>

#include <kc/assert.h>
#include <kc/stdlib.h>
#include <kc/string.h>

#include <opal/mm/buddy.h>
#include <opal/mm/pfn.h>
#include <opal/mm/slab.h>
#include <opal/platform/mm/defines.h>

#define SLAB_REDZONE_SIZE 16
#define SLAB_REDZONE_PATTERN 0xfd
#define SLAB_UNUSED_PATTERN 0xcc

// page: [struct slab_page][object 1][object 2]...
// object: [struct slab_obj_hdr][redzone][payload][redzone]

struct slab_obj_hdr {
    struct slab_obj_hdr *next_free;
    bool is_free;
};

struct slab_page {
    struct linkedlist_link link;
    struct slab *owner;
    uint32_t inuse;
    struct slab_obj_hdr *free_head;
};

static size_t max_size(size_t a, size_t b) {
    return a > b ? a : b;
}

static struct slab_page *page_from_link(struct linkedlist_link *link) {
    return container_of(link, struct slab_page, link);
}

static struct slab_obj_hdr *slot_object(const struct slab *slab, const struct slab_page *page, uint32_t slot_idx) {
    size_t offset = slab->slot_offset + slab->slot_stride * slot_idx;
    return (struct slab_obj_hdr *)((char *)page + offset);
}

static struct slab_page *hdr_to_page(const struct slab_obj_hdr *hdr) {
    return (struct slab_page *)align_floor_sz_p2((size_t)(uintptr_t)hdr, PAGE_SIZE);
}

static void *payload_ptr(const struct slab *slab, const struct slab_obj_hdr *hdr) {
    return (char *)hdr + slab->payload_offset;
}

static struct slab_obj_hdr *slot_from_payload(const struct slab *slab, const void *payload) {
    return (struct slab_obj_hdr *)((char *)payload - slab->payload_offset);
}

struct redzones {
    char *prefix;
    size_t prefix_len;
    char *suffix;
    size_t suffix_len;
};

static struct redzones get_redzones(const struct slab *slab, const struct slab_obj_hdr *hdr) {
    const size_t suffix_offset = slab->payload_offset + slab->object_size;
    return (struct redzones){
        .prefix = (char *)hdr + sizeof(*hdr),
        .prefix_len = slab->payload_offset - sizeof(*hdr),
        .suffix = (char *)hdr + suffix_offset,
        .suffix_len = slab->slot_stride - suffix_offset,
    };
}

static void fill_redzones(const struct slab *slab, struct slab_obj_hdr *hdr) {
    struct redzones redzones = get_redzones(slab, hdr);

    memset(redzones.prefix, SLAB_REDZONE_PATTERN, redzones.prefix_len);
    memset(redzones.suffix, SLAB_REDZONE_PATTERN, redzones.suffix_len);
}

static void check_redzones(const struct slab *slab, const struct slab_obj_hdr *hdr) {
    struct redzones redzones = get_redzones(slab, hdr);

    void *const prefix_bad = memchr_not(redzones.prefix, SLAB_REDZONE_PATTERN, redzones.prefix_len);
    assert(!prefix_bad, "slab redzone prefix corrupted");

    void *const suffix_bad = memchr_not(redzones.suffix, SLAB_REDZONE_PATTERN, redzones.suffix_len);
    assert(!suffix_bad, "slab redzone suffix corrupted");
}

static void fill_unused_payload(const struct slab *slab, struct slab_obj_hdr *hdr) {
    memset(payload_ptr(slab, hdr), SLAB_UNUSED_PATTERN, slab->object_size);
}

static void check_unused_payload(const struct slab *slab, const struct slab_obj_hdr *hdr) {
    void *const unused_bad = memchr_not(payload_ptr(slab, hdr), SLAB_UNUSED_PATTERN, slab->object_size);
    assert(!unused_bad, "slab unused payload corrupted");
}

static void push_partial_page(struct slab *slab, struct slab_page *page) {
    linkedlist_push_front(&slab->partial_pages, &page->link);
}

static struct slab_page *pop_partial_page(struct slab *slab) {
    struct linkedlist_link *link = linkedlist_pop_front(&slab->partial_pages);
    if (!link) {
        return NULL;
    }
    return page_from_link(link);
}

static struct slab_page *create_slab_page(struct slab *slab) {
    pfn_t pfn = mm_buddy_alloc(0);
    if (pfn == PFN_INVALID) {
        return NULL;
    }

    struct slab_page *page = mm_pfn_to_ptr(pfn);
    memset(page, 0, sizeof(*page));
    page->owner = slab;

    for (uint32_t i = 0; i < slab->page_capacity; i++) {
        struct slab_obj_hdr *hdr = slot_object(slab, page, i);
        hdr->next_free = page->free_head;
        hdr->is_free = true;
        page->free_head = hdr;

        fill_redzones(slab, hdr);
        fill_unused_payload(slab, hdr);
    }

    slab->total_objects += slab->page_capacity;
    return page;
}

static void destroy_slab_page(struct slab *slab, struct slab_page *page) {
    assert(page->inuse == 0, "cannot destroy in-use slab page");
    slab->total_objects -= slab->page_capacity;
    mm_buddy_free(mm_ptr_to_pfn(page), 0);
}

static struct slab_page *pick_alloc_page(struct slab *slab) {
    if (linkedlist_is_empty(&slab->partial_pages)) {
        struct slab_page *page = create_slab_page(slab);
        if (!page) {
            return NULL;
        }
        push_partial_page(slab, page);
    }

    struct linkedlist_link *head = linkedlist_head(&slab->partial_pages);
    assert(head != linkedlist_nil(&slab->partial_pages));
    struct slab_page *page = page_from_link(head);
    assert(page->free_head != NULL, "partial slab page has no free objects");
    return page;
}

void slab_create(struct slab *slab, size_t object_size, size_t object_align) {
    assert(slab);
    assert(!slab->initialized, "slab is already initialized");
    assert(0 < object_size && object_size <= UINT16_MAX, "invalid slab object_size");
    assert(ispower2(object_align) && object_align <= UINT16_MAX, "invalid slab object_align");

    memset(slab, 0, sizeof(*slab));
    slab->object_size = (uint16_t)object_size;
    slab->object_align = (uint16_t)object_align;

    const size_t slot_align = max_size(object_align, alignof(struct slab_obj_hdr));
    const size_t prefix_end = align_ceil_sz_p2(sizeof(struct slab_obj_hdr) + SLAB_REDZONE_SIZE, object_align);
    const size_t slot_data_size = prefix_end + object_size + SLAB_REDZONE_SIZE;
    const size_t slot_stride = align_ceil_sz_p2(slot_data_size, slot_align);

    const size_t slot_offset = align_ceil_sz_p2(sizeof(struct slab_page), slot_align);
    const size_t page_capacity = (PAGE_SIZE - slot_offset) / slot_stride;

    assert(slot_stride <= UINT16_MAX, "slab object is too big");
    assert(slot_offset < PAGE_SIZE, "page is too small");
    assert(page_capacity > 0, "slab object is too big");
    assert(page_capacity <= UINT16_MAX, "page is too small");

    slab->payload_offset = (uint16_t)prefix_end;
    slab->slot_stride = (uint16_t)slot_stride;
    slab->slot_offset = (uint32_t)slot_offset;
    slab->page_capacity = (uint32_t)page_capacity;

    linkedlist_init(&slab->partial_pages);
    slab->initialized = true;
}

void slab_destroy(struct slab *slab) {
    assert(slab && slab->initialized, "slab is not initialized");
    assert(slab->inuse_objects == 0, "slab has in-use objects");

    struct slab_page *page;
    while ((page = pop_partial_page(slab)) != NULL) {
        destroy_slab_page(slab, page);
    }

    memset(slab, 0, sizeof(*slab));
}

void *slab_alloc(struct slab *slab) {
    assert(slab && slab->initialized, "slab is not initialized");

    struct slab_page *page = pick_alloc_page(slab);
    if (!page) {
        return NULL;
    }

    struct slab_obj_hdr *hdr = page->free_head;
    assert(hdr, "slab page has no free objects");
    assert(hdr->is_free, "slab object state is corrupted");

    check_redzones(slab, hdr);
    check_unused_payload(slab, hdr);

    page->free_head = hdr->next_free;
    hdr->next_free = NULL;
    hdr->is_free = false;
    page->inuse++;
    slab->inuse_objects++;

    if (page->free_head == NULL) {
        linkedlist_remove(&page->link);
    }

    void *payload = payload_ptr(slab, hdr);
    memset(payload, 0, slab->object_size);
    return payload;
}

void slab_free(struct slab *slab, void *ptr) {
    assert(slab && slab->initialized, "slab is not initialized");
    assert(ptr, "slab free ptr is null");

    const virt_addr_t va = (virt_addr_t)ptr;
    assert(va >= DIRECT_MAP_START_VIRT && va < DIRECT_MAP_END_VIRT, "invalid pointer to free");

    struct slab_obj_hdr *hdr = slot_from_payload(slab, ptr);
    struct slab_page *page = hdr_to_page(hdr);

    size_t offset = (char *)hdr - (char *)page;
    assert(offset >= slab->slot_offset, "invalid pointer to free");
    assert((offset - slab->slot_offset) % slab->slot_stride == 0, "invalid pointer to free");

    assert(page->owner == slab, "object belongs to another slab");
    assert(!hdr->is_free, "double free detected");
    assert(page->inuse > 0, "slab page is corrupted");

    const bool was_full = (page->free_head == NULL);

    check_redzones(slab, hdr);
    fill_unused_payload(slab, hdr);
    fill_redzones(slab, hdr);

    hdr->is_free = true;
    hdr->next_free = page->free_head;
    page->free_head = hdr;

    page->inuse--;
    slab->inuse_objects--;

    if (was_full) {
        push_partial_page(slab, page);
    }

    if (page->inuse == 0) {
        linkedlist_remove(&page->link);
        destroy_slab_page(slab, page);
    }
}

size_t slab_get_object_size(const struct slab *slab) {
    assert(slab && slab->initialized, "slab is not initialized");
    return slab->object_size;
}

size_t slab_get_inuse(const struct slab *slab) {
    assert(slab && slab->initialized, "slab is not initialized");
    return slab->inuse_objects;
}

size_t slab_get_total(const struct slab *slab) {
    assert(slab && slab->initialized, "slab is not initialized");
    return slab->total_objects;
}
