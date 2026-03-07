#pragma once

#include <kc/stdlib.h>

enum rbtree_color {
    RBTREE_RED,
    RBTREE_BLACK,
};

struct rbtree_node {
    enum rbtree_color color;
    struct rbtree_node* parent;
    struct rbtree_node* left;
    struct rbtree_node* right;
};

struct rbtree {
    struct rbtree_node* root;
};

struct rbtree_find_result {
    struct rbtree_node* lower;
    struct rbtree_node* upper;
};

void rbtree_init(struct rbtree* tree);

[[nodiscard]] struct rbtree_node* rbtree_first(struct rbtree* tree);
[[nodiscard]] struct rbtree_node* rbtree_next(struct rbtree_node* node);

void rbtree_link_insert(struct rbtree* tree, struct rbtree_node* parent, struct rbtree_node** link, struct rbtree_node* node);
void rbtree_remove(struct rbtree* tree, struct rbtree_node* node);

#define RBTREE_INSERT_TEMPLATE(type, node_, comp, postfix, ...) \
    __VA_ARGS__ \
    void rbtree_insert_##postfix(struct rbtree* tree, type* data) { \
        struct rbtree_node **link = &tree->root; \
        struct rbtree_node *parent = NULL; \
        while (*link) { \
            type *found = container_of(*link, type, node_); \
            int order = (comp)(found, data); \
            parent = *link; \
            if (order < 0) { \
                link = &(*link)->right; \
            } else if (order > 0) { \
                link = &(*link)->left; \
            } else { \
                return; \
            } \
        } \
        rbtree_link_insert(tree, parent, link, &data->node_); \
    }

#define RBTREE_TEMPLATE(type, keytype, node_, comp, comp_to, postfix, ...) \
    RBTREE_INSERT_TEMPLATE(type, node_, comp, postfix, ##__VA_ARGS__) \
    [[nodiscard]] __VA_ARGS__ \
    struct rbtree_find_result rbtree_find_##postfix(struct rbtree* tree, keytype key) { \
        struct rbtree_find_result result = { .lower = NULL, .upper = NULL }; \
        struct rbtree_node* node = tree->root; \
        while (node) { \
            type *found = container_of(node, type, node_); \
            int order = (comp_to)(found, key); \
            if (order < 0) { \
                result.lower = node; \
                node = node->right; \
            } else if (order > 0) { \
                result.upper = node; \
                node = node->left; \
            } else { \
                result.lower = result.upper = node; \
                return result; \
            } \
        } \
        return result; \
    }
