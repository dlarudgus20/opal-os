#include <kc/kassert.h>

#include <opal/utils/xarray.h>
#include <opal/mm/kmalloc.h>

// TODO: (1ull << shift) * xa->stride overflow

#define FLAG_MASK   ((uint64_t)1)
#define FLAG_VALUE  ((uint64_t)1)

static_assert(sizeof(uint64_t) >= sizeof(uintptr_t));

static uint64_t shr64(uint64_t a, uint64_t b) {
    if (b >= 64) {
        return 0;
    }
    return a >> b;
}

static bool addmul_u64_checked(uint64_t base, uint64_t mul_a, uint64_t mul_b, uint64_t *out) {
    if (mul_a != 0 && mul_b > UINT64_MAX / mul_a) {
        return true;
    }

    uint64_t delta = mul_a * mul_b;
    if (base > UINT64_MAX - delta) {
        return true;
    }

    *out = base + delta;
    return false;
}

void xarray_init(struct xarray *xa, uintptr_t stride) {
    kassert(stride > 0 && (stride & FLAG_MASK) == 0);
    xa->root = (struct xa_node){};
    xa->stride = stride;
    xa->depth = 1;
}

static void destroy_node(struct xa_node *node, unsigned shift) {
    if (shift == 0) {
        return;
    }

    for (uint64_t i = 0; i < XARRAY_SLOT_SIZE; i++) {
        uint64_t slot = node->slots[i];
        if ((slot & FLAG_VALUE) != 0) {
            continue;
        }

        struct xa_node *child = (struct xa_node *)(slot & ~FLAG_MASK);
        if (!child) {
            continue;
        }

        destroy_node(child, shift - XARRAY_SLOT_BITS);
        kfree(child, sizeof(*child));
    }
}

void xarray_destroy(struct xarray *xa) {
    if (xa->depth > 1) {
        destroy_node(&xa->root, (xa->depth - 1) * XARRAY_SLOT_BITS);
    }
    xa->root = (struct xa_node){};
    xa->depth = 1;
}

void *xarray_get(const struct xarray *xa, uint64_t index) {
    unsigned shift = xa->depth * XARRAY_SLOT_BITS;
    if (shr64(index, shift) != 0) {
        return NULL;
    }

    const struct xa_node *node = &xa->root;
    while (shift > 0) {
        shift -= XARRAY_SLOT_BITS;

        uint64_t slot = node->slots[shr64(index, shift) & (XARRAY_SLOT_SIZE - 1)];
        uint64_t sv = slot & ~FLAG_MASK;
        if (slot & FLAG_VALUE) {
            return (void *)(sv + (index & ((1ull << shift) - 1)) * xa->stride);
        }

        node = (const struct xa_node *)sv;
        if (!node) {
            return NULL;
        }
    }
    panic();
}

static bool ensure_depth(struct xarray *xa, uint64_t index) {
    while (1) {
        unsigned shift = xa->depth * XARRAY_SLOT_BITS;
        if (shr64(index, shift) == 0) {
            return true;
        }

        struct xa_node *old_root = kzalloc(sizeof(*old_root));
        if (!old_root) {
            return false;
        }

        *old_root = xa->root;
        xa->root = (struct xa_node){};
        xa->root.slots[0] = (uint64_t)old_root;
        xa->depth++;
    }
}

static struct xa_node *expand_slot(uint64_t *slot, uint64_t stride) {
    struct xa_node *node = kzalloc(sizeof(*node));
    if (!node) {
        return NULL;
    }
    uint64_t value = *slot & ~FLAG_MASK;
    for (uint64_t i = 0; i < XARRAY_SLOT_SIZE; i++) {
        node->slots[i] = value;
        value += stride;
    }
    *slot = (uint64_t)node;
    return node;
}

bool xarray_set(struct xarray *xa, uint64_t index, void *value) {
    if (!ensure_depth(xa, index)) {
        return false;
    }

    unsigned shift = xa->depth * XARRAY_SLOT_BITS;
    struct xa_node *node = &xa->root;
    while (shift > 0) {
        shift -= XARRAY_SLOT_BITS;

        uint64_t *slot = &node->slots[shr64(index, shift) & (XARRAY_SLOT_SIZE - 1)];
        if (shift == 0) {
            *slot = (uint64_t)value | FLAG_VALUE;
            return true;
        }

        uint64_t sv = *slot & ~FLAG_MASK;
        if (*slot & FLAG_VALUE) {
            uint64_t stored = sv + (index & ((1ull << shift) - 1)) * xa->stride;
            if ((uint64_t)value == stored) {
                return true;
            }
            node = expand_slot(slot, (1ull << shift) * xa->stride);
            if (!node) {
                return false;
            }
            continue;
        }

        node = (struct xa_node *)sv;
        if (node) {
            continue;
        }

        node = kzalloc(sizeof(*node));
        if (!node) {
            return false;
        }
        *slot = (uint64_t)node;
    }
    panic();
}

static bool value_for_index(const struct xarray *xa, uint64_t start_index, uint64_t start_value,
    uint64_t at, uint64_t *out) {
    kassert(at >= start_index);
    return !addmul_u64_checked(start_value, at - start_index, xa->stride, out);
}

static bool set_range_node(struct xarray *xa, struct xa_node *node, uint64_t node_base,
    unsigned slot_shift, uint64_t range_first, uint64_t range_last, uint64_t start_index,
    uint64_t start_value) {
    uint64_t slot_span = 1ull << slot_shift;
    uint64_t slot_last_delta = slot_span - 1;

    for (uint64_t i = 0; i < XARRAY_SLOT_SIZE; i++) {
        uint64_t slot_first;
        if (addmul_u64_checked(node_base, i, slot_span, &slot_first)) {
            break;
        }

        if (range_last < slot_first) {
            break;
        }

        uint64_t slot_last = slot_first + slot_last_delta;
        if (slot_last < slot_first) {
            slot_last = UINT64_MAX;
        }
        if (slot_last < range_first) {
            continue;
        }

        uint64_t *slot = &node->slots[i];
        bool full_cover = range_first <= slot_first && slot_last <= range_last;
        if (full_cover || slot_shift == 0) {
            uint64_t value;
            if (!value_for_index(xa, start_index, start_value, slot_first, &value)) {
                return false;
            }
            *slot = value | FLAG_VALUE;
            continue;
        }

        struct xa_node *child;
        if (*slot & FLAG_VALUE) {
            child = expand_slot(slot, slot_span * xa->stride);
            if (!child) {
                return false;
            }
        } else {
            child = (struct xa_node *)(*slot & ~FLAG_MASK);
            if (!child) {
                child = kzalloc(sizeof(*child));
                if (!child) {
                    return false;
                }
                *slot = (uint64_t)child;
            }
        }

        if (!set_range_node(xa, child, slot_first, slot_shift - XARRAY_SLOT_BITS, range_first,
                range_last, start_index, start_value)) {
            return false;
        }
    }
    return true;
}

bool xarray_set_range(struct xarray *xa, uint64_t index, uint64_t len, void *start) {
    if (len == 0) {
        return true;
    }

    uint64_t last;
    if (addmul_u64_checked(index, len - 1, 1, &last)) {
        return false;
    }
    if (!ensure_depth(xa, last)) {
        return false;
    }

    return set_range_node(
        xa, &xa->root, 0, (xa->depth - 1) * XARRAY_SLOT_BITS, index, last, index, (uintptr_t)start);
}
