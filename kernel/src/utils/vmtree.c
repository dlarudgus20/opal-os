#include <opal/utils/vmtree.h>
#include <opal/mm/kmalloc.h>

#include <kc/assert.h>
#include <kc/string.h>

#define VMTREE_FLAG_LEAF ((uintptr_t)1)
#define VMTREE_FLAG_MASK (KMALLOC_ALIGN - 1)

enum vmtree_write_mode {
    VMTREE_WRITE_SET,
    VMTREE_WRITE_REMOVE,
};

static void fill_unused_pivots(struct vmtree_node *node, size_t count) {
    for (size_t i = count; i < VMTREE_NODE_SLOTS - 1; i++) {
        node->pivots[i] = UINTPTR_MAX;
    }
}

static size_t pivot_count(const struct vmtree_node *node) {
    for (size_t i = 0; i < VMTREE_NODE_SLOTS - 1; i++) {
        if (node->pivots[i] == UINTPTR_MAX) {
            return i;
        }
    }
    return VMTREE_NODE_SLOTS - 1;
}

static bool node_is_full(const struct vmtree_node *node) {
    return node->pivots[VMTREE_NODE_SLOTS - 2] != UINTPTR_MAX;
}

static uintptr_t node_tag(struct vmtree_node *node, bool leaf) {
    assert(((uintptr_t)node & VMTREE_FLAG_MASK) == 0);
    return (uintptr_t)node | (leaf ? VMTREE_FLAG_LEAF : 0);
}

static struct vmtree_node *node_ptr(uintptr_t tagged) {
    return (struct vmtree_node *)(tagged & ~VMTREE_FLAG_MASK);
}

static bool node_is_leaf(uintptr_t tagged) {
    return (tagged & VMTREE_FLAG_LEAF) != 0;
}

static size_t slot_for_addr(const struct vmtree_node *node, uintptr_t addr) {
    const size_t n = pivot_count(node);
    size_t i = 0;
    while (i < n && addr >= node->pivots[i]) {
        i++;
    }
    return i;
}

static uintptr_t slot_lo(const struct vmtree_node *node, size_t slot, uintptr_t lo) {
    if (slot == 0) {
        return lo;
    }
    return node->pivots[slot - 1];
}

static uintptr_t slot_hi(const struct vmtree_node *node, size_t slot, uintptr_t hi) {
    const size_t n = pivot_count(node);
    if (slot >= n) {
        return hi;
    }
    return node->pivots[slot];
}

static struct vmtree_node *alloc_leaf_node(void *entry, struct vmtree_node *parent) {
    struct vmtree_node *node = kzalloc(sizeof(*node));
    if (!node) {
        return NULL;
    }
    node->parent = parent;
    fill_unused_pivots(node, 0);
    node->slots[0] = (uintptr_t)entry;
    return node;
}

static void free_subtree(uintptr_t tagged, bool free_self) {
    struct vmtree_node *node = node_ptr(tagged);
    if (!node) {
        return;
    }

    if (!node_is_leaf(tagged)) {
        const size_t n = pivot_count(node);
        for (size_t i = 0; i <= n; i++) {
            free_subtree(node->slots[i], true);
        }
    }

    if (free_self) {
        kfree(node, sizeof(*node));
    }
}

static bool child_is_uniform(uintptr_t tagged, uintptr_t *entry_out) {
    if (!node_is_leaf(tagged)) {
        return false;
    }

    const struct vmtree_node *node = node_ptr(tagged);
    if (pivot_count(node) != 0) {
        return false;
    }

    *entry_out = node->slots[0];
    return true;
}

static void remove_leaf_pivot(struct vmtree_node *node, size_t pivot_idx) {
    const size_t n = pivot_count(node);
    assert(pivot_idx < n);
    assert(n > 0);

    for (size_t i = pivot_idx; i + 1 < n; i++) {
        node->pivots[i] = node->pivots[i + 1];
    }
    fill_unused_pivots(node, n - 1);

    for (size_t i = pivot_idx + 1; i < n; i++) {
        node->slots[i] = node->slots[i + 1];
    }
}

static void merge_leaf_neighbors(struct vmtree_node *node) {
    size_t i = 0;
    while (i < pivot_count(node)) {
        if (node->slots[i] == node->slots[i + 1]) {
            remove_leaf_pivot(node, i);
            continue;
        }
        i++;
    }
}

static void remove_internal_pivot(struct vmtree_node *node, size_t pivot_idx) {
    const size_t n = pivot_count(node);
    assert(pivot_idx < n);
    assert(n > 0);

    for (size_t i = pivot_idx; i + 1 < n; i++) {
        node->pivots[i] = node->pivots[i + 1];
    }
    fill_unused_pivots(node, n - 1);

    for (size_t i = pivot_idx + 1; i < n; i++) {
        node->slots[i] = node->slots[i + 1];
    }
}

static void collapse_to_leaf(uintptr_t *tagged_ref, uintptr_t entry) {
    struct vmtree_node *node = node_ptr(*tagged_ref);
    node->parent = node->parent;
    memset(node->slots, 0, sizeof(node->slots));
    fill_unused_pivots(node, 0);
    node->slots[0] = entry;
    *tagged_ref = node_tag(node, true);
}

static vmtree_status_t promote_leaf_to_internal(uintptr_t *tagged_ref) {
    struct vmtree_node *node = node_ptr(*tagged_ref);
    const size_t n = pivot_count(node);
    struct vmtree_node *children[VMTREE_NODE_SLOTS] = { 0 };

    for (size_t i = 0; i <= n; i++) {
        children[i] = alloc_leaf_node((void *)node->slots[i], node);
        if (!children[i]) {
            for (size_t j = 0; j < i; j++) {
                kfree(children[j], sizeof(*children[j]));
            }
            return VMTREE_ERR_NOMEM;
        }
    }

    for (size_t i = 0; i <= n; i++) {
        node->slots[i] = node_tag(children[i], true);
    }

    *tagged_ref = node_tag(node, false);
    return VMTREE_OK;
}

static vmtree_status_t insert_leaf_boundary(struct vmtree_node *node, uintptr_t lo, uintptr_t hi, uintptr_t x) {
    const size_t n = pivot_count(node);
    const size_t slot = slot_for_addr(node, x);
    const uintptr_t left = slot_lo(node, slot, lo);
    const uintptr_t right = slot_hi(node, slot, hi);
    if (x == left || x == right) {
        return VMTREE_OK;
    }
    if (node_is_full(node)) {
        return VMTREE_ERR_NOMEM;
    }

    for (size_t i = n; i > slot; i--) {
        node->pivots[i] = node->pivots[i - 1];
    }
    node->pivots[slot] = x;
    fill_unused_pivots(node, n + 1);

    for (size_t i = n + 1; i > slot + 1; i--) {
        node->slots[i] = node->slots[i - 1];
    }
    node->slots[slot + 1] = node->slots[slot];
    return VMTREE_OK;
}

static vmtree_status_t ensure_boundary_rec(uintptr_t *tagged_ref, uintptr_t lo, uintptr_t hi, uintptr_t x) {
    if (x <= lo || x >= hi) {
        return VMTREE_OK;
    }

    struct vmtree_node *node = node_ptr(*tagged_ref);
    if (node_is_leaf(*tagged_ref)) {
        for (;;) {
            vmtree_status_t st = insert_leaf_boundary(node, lo, hi, x);
            if (st == VMTREE_OK) {
                return VMTREE_OK;
            }

            st = promote_leaf_to_internal(tagged_ref);
            if (st != VMTREE_OK) {
                return st;
            }
            break;
        }
    }

    node = node_ptr(*tagged_ref);
    const size_t slot = slot_for_addr(node, x);
    const uintptr_t child_lo = slot_lo(node, slot, lo);
    const uintptr_t child_hi = slot_hi(node, slot, hi);
    return ensure_boundary_rec(&node->slots[slot], child_lo, child_hi, x);
}

static bool any_nonhole_rec(uintptr_t tagged, uintptr_t lo, uintptr_t hi, uintptr_t start, uintptr_t end) {
    if (end <= lo || hi <= start) {
        return false;
    }

    struct vmtree_node *node = node_ptr(tagged);
    if (node_is_leaf(tagged)) {
        const size_t n = pivot_count(node);
        for (size_t i = 0; i <= n; i++) {
            const uintptr_t r_lo = slot_lo(node, i, lo);
            const uintptr_t r_hi = slot_hi(node, i, hi);
            if (end <= r_lo || r_hi <= start) {
                continue;
            }
            if (node->slots[i] != 0) {
                return true;
            }
        }
        return false;
    }

    const size_t n = pivot_count(node);
    for (size_t i = 0; i <= n; i++) {
        const uintptr_t r_lo = slot_lo(node, i, lo);
        const uintptr_t r_hi = slot_hi(node, i, hi);
        if (end <= r_lo || r_hi <= start) {
            continue;
        }
        if (any_nonhole_rec(node->slots[i], r_lo, r_hi, start, end)) {
            return true;
        }
    }
    return false;
}

static bool any_hole_rec(uintptr_t tagged, uintptr_t lo, uintptr_t hi, uintptr_t start, uintptr_t end) {
    if (end <= lo || hi <= start) {
        return false;
    }

    struct vmtree_node *node = node_ptr(tagged);
    if (node_is_leaf(tagged)) {
        const size_t n = pivot_count(node);
        for (size_t i = 0; i <= n; i++) {
            const uintptr_t r_lo = slot_lo(node, i, lo);
            const uintptr_t r_hi = slot_hi(node, i, hi);
            if (end <= r_lo || r_hi <= start) {
                continue;
            }
            if (node->slots[i] == 0) {
                return true;
            }
        }
        return false;
    }

    const size_t n = pivot_count(node);
    for (size_t i = 0; i <= n; i++) {
        const uintptr_t r_lo = slot_lo(node, i, lo);
        const uintptr_t r_hi = slot_hi(node, i, hi);
        if (end <= r_lo || r_hi <= start) {
            continue;
        }
        if (any_hole_rec(node->slots[i], r_lo, r_hi, start, end)) {
            return true;
        }
    }
    return false;
}

static void set_subtree_uniform(uintptr_t *tagged_ref, uintptr_t entry) {
    struct vmtree_node *node = node_ptr(*tagged_ref);
    if (!node_is_leaf(*tagged_ref)) {
        const size_t n = pivot_count(node);
        for (size_t i = 0; i <= n; i++) {
            free_subtree(node->slots[i], true);
        }
    }

    memset(node->slots, 0, sizeof(node->slots));
    fill_unused_pivots(node, 0);
    node->slots[0] = entry;
    *tagged_ref = node_tag(node, true);
}

static void normalize_rec(uintptr_t *tagged_ref) {
    struct vmtree_node *node = node_ptr(*tagged_ref);
    if (node_is_leaf(*tagged_ref)) {
        merge_leaf_neighbors(node);
        return;
    }

    size_t n = pivot_count(node);
    for (size_t i = 0; i <= n; i++) {
        normalize_rec(&node->slots[i]);
    }

    size_t i = 0;
    while (i < n) {
        uintptr_t left_entry = 0;
        uintptr_t right_entry = 0;
        if (!child_is_uniform(node->slots[i], &left_entry) ||
            !child_is_uniform(node->slots[i + 1], &right_entry) ||
            left_entry != right_entry) {
            i++;
            continue;
        }

        free_subtree(node->slots[i + 1], true);
        remove_internal_pivot(node, i);
        n--;
    }

    if (n == 0) {
        uintptr_t e = 0;
        if (child_is_uniform(node->slots[0], &e)) {
            free_subtree(node->slots[0], true);
            collapse_to_leaf(tagged_ref, e);
        }
    }
}

static void write_range_rec(
    uintptr_t *tagged_ref, uintptr_t lo, uintptr_t hi, uintptr_t start, uintptr_t end, uintptr_t entry
) {
    if (end <= lo || hi <= start) {
        return;
    }

    if (start <= lo && hi <= end) {
        set_subtree_uniform(tagged_ref, entry);
        return;
    }

    struct vmtree_node *node = node_ptr(*tagged_ref);
    if (node_is_leaf(*tagged_ref)) {
        const size_t n = pivot_count(node);
        for (size_t i = 0; i <= n; i++) {
            const uintptr_t r_lo = slot_lo(node, i, lo);
            const uintptr_t r_hi = slot_hi(node, i, hi);
            if (end <= r_lo || r_hi <= start) {
                continue;
            }
            node->slots[i] = entry;
        }
        merge_leaf_neighbors(node);
        return;
    }

    const size_t n = pivot_count(node);
    for (size_t i = 0; i <= n; i++) {
        const uintptr_t r_lo = slot_lo(node, i, lo);
        const uintptr_t r_hi = slot_hi(node, i, hi);
        if (end <= r_lo || r_hi <= start) {
            continue;
        }
        write_range_rec(&node->slots[i], r_lo, r_hi, start, end, entry);
    }
}

static vmtree_status_t validate_range(uintptr_t root, uintptr_t start, uintptr_t end, enum vmtree_write_mode mode) {
    if (mode == VMTREE_WRITE_REMOVE) {
        return VMTREE_OK;
    }

    if (mode == VMTREE_WRITE_SET) {
        return any_hole_rec(root, 0, UINTPTR_MAX, start, end) ? VMTREE_ERR_NOENT : VMTREE_OK;
    }

    panic("unexpected write mode");
}

static vmtree_status_t insert_validate(uintptr_t root, uintptr_t start, uintptr_t end) {
    return any_nonhole_rec(root, 0, UINTPTR_MAX, start, end) ? VMTREE_ERR_EXISTS : VMTREE_OK;
}

static vmtree_status_t write_range(
    struct vmtree *tree, uintptr_t start, uintptr_t end, uintptr_t entry, enum vmtree_write_mode mode
) {
    if (start > end) {
        return VMTREE_ERR_INVAL;
    }
    if (start == end) {
        return VMTREE_OK;
    }

    vmtree_status_t st = validate_range(tree->root, start, end, mode);
    if (st != VMTREE_OK) {
        return st;
    }

    st = ensure_boundary_rec(&tree->root, 0, UINTPTR_MAX, start);
    if (st != VMTREE_OK) {
        return st;
    }
    st = ensure_boundary_rec(&tree->root, 0, UINTPTR_MAX, end);
    if (st != VMTREE_OK) {
        return st;
    }

    write_range_rec(&tree->root, 0, UINTPTR_MAX, start, end, entry);
    normalize_rec(&tree->root);
    return VMTREE_OK;
}

void vmtree_init(struct vmtree *tree) {
    assert(tree);
    tree->root_node.parent = NULL;
    for (size_t i = 0; i < VMTREE_NODE_SLOTS; i++) {
        tree->root_node.slots[i] = 0;
    }
    fill_unused_pivots(&tree->root_node, 0);
    tree->root_node.slots[0] = 0;
    tree->root = node_tag(&tree->root_node, true);
}

void vmtree_destroy(struct vmtree *tree) {
    if (!tree) {
        return;
    }

    free_subtree(tree->root, false);
    vmtree_init(tree);
}

struct vmtree_entry vmtree_get(struct vmtree *tree, uintptr_t addr) {
    if (!tree || addr == UINTPTR_MAX) {
        return (struct vmtree_entry){ .start = addr, .end = addr, .entry = NULL };
    }

    uintptr_t tagged = tree->root;
    uintptr_t lo = 0;
    uintptr_t hi = UINTPTR_MAX;

    for (;;) {
        struct vmtree_node *node = node_ptr(tagged);
        const size_t slot = slot_for_addr(node, addr);
        lo = slot_lo(node, slot, lo);
        hi = slot_hi(node, slot, hi);

        if (node_is_leaf(tagged)) {
            return (struct vmtree_entry){
                .start = lo,
                .end = hi,
                .entry = (void *)node->slots[slot],
            };
        }

        tagged = node->slots[slot];
    }
}

vmtree_status_t vmtree_insert(struct vmtree *tree, uintptr_t start, uintptr_t end, void *entry) {
    if (!tree || !entry) {
        return VMTREE_ERR_INVAL;
    }
    if (start > end) {
        return VMTREE_ERR_INVAL;
    }
    if (start == end) {
        return VMTREE_OK;
    }

    vmtree_status_t st = insert_validate(tree->root, start, end);
    if (st != VMTREE_OK) {
        return st;
    }

    st = ensure_boundary_rec(&tree->root, 0, UINTPTR_MAX, start);
    if (st != VMTREE_OK) {
        return st;
    }
    st = ensure_boundary_rec(&tree->root, 0, UINTPTR_MAX, end);
    if (st != VMTREE_OK) {
        return st;
    }

    write_range_rec(&tree->root, 0, UINTPTR_MAX, start, end, (uintptr_t)entry);
    normalize_rec(&tree->root);
    return VMTREE_OK;
}

vmtree_status_t vmtree_set(struct vmtree *tree, uintptr_t start, uintptr_t end, void *entry) {
    if (!tree || !entry) {
        return VMTREE_ERR_INVAL;
    }
    return write_range(tree, start, end, (uintptr_t)entry, VMTREE_WRITE_SET);
}

vmtree_status_t vmtree_remove(struct vmtree *tree, uintptr_t start, uintptr_t end) {
    if (!tree) {
        return VMTREE_ERR_INVAL;
    }
    return write_range(tree, start, end, 0, VMTREE_WRITE_REMOVE);
}

struct vmtree_iter vmtree_before_begin(struct vmtree *tree) {
    return (struct vmtree_iter){
        .tree = tree,
        .next_addr = 0,
        .finished = tree == NULL,
    };
}

bool vmtree_iter_next(struct vmtree_iter *iter, struct vmtree_entry *out) {
    if (!iter || !out || iter->finished || !iter->tree) {
        return false;
    }

    uintptr_t addr = iter->next_addr;
    while (1) {
        if (addr == UINTPTR_MAX) {
            iter->finished = true;
            return false;
        }

        struct vmtree_entry entry = vmtree_get(iter->tree, addr);
        assert(entry.end > addr);

        if (entry.entry) {
            *out = entry;
            if (entry.end == UINTPTR_MAX) {
                iter->finished = true;
            } else {
                iter->next_addr = entry.end;
            }
            return true;
        }

        if (entry.end == UINTPTR_MAX) {
            iter->finished = true;
            return false;
        }
        addr = entry.end;
    }
}
