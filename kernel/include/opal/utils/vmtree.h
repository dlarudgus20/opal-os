#ifndef OPAL_UTILS_VMTREE_H
#define OPAL_UTILS_VMTREE_H

#include <stdbool.h>
#include <stdint.h>

#define VMTREE_NODE_SLOTS 16

typedef enum vmtree_status {
    VMTREE_OK,
    VMTREE_ERR_NOENT,
    VMTREE_ERR_EXISTS,
    VMTREE_ERR_INVAL,
    VMTREE_ERR_NOMEM,
} vmtree_status_t;

struct vmtree_node {
    struct vmtree_node *parent;
    uintptr_t pivots[VMTREE_NODE_SLOTS - 1];
    uintptr_t slots[VMTREE_NODE_SLOTS];
};

struct vmtree {
    struct vmtree_node root_node;
    uintptr_t root;
};

struct vmtree_entry {
    uintptr_t start;
    uintptr_t end;
    void *entry;
};

struct vmtree_iter {
    struct vmtree *tree;
    uintptr_t next_addr;
    bool finished;
};

void vmtree_init(struct vmtree *tree);
void vmtree_destroy(struct vmtree *tree);
[[nodiscard]] struct vmtree_entry vmtree_get(struct vmtree *tree, uintptr_t addr);
vmtree_status_t vmtree_insert(struct vmtree *tree, uintptr_t start, uintptr_t end, void *entry);
vmtree_status_t vmtree_set(struct vmtree *tree, uintptr_t start, uintptr_t end, void *entry);
vmtree_status_t vmtree_remove(struct vmtree *tree, uintptr_t start, uintptr_t end);

[[nodiscard]] struct vmtree_iter vmtree_before_begin(struct vmtree *tree);
[[nodiscard]] bool vmtree_iter_next(struct vmtree_iter *iter, struct vmtree_entry *out);

#endif
