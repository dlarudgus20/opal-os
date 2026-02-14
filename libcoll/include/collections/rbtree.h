#pragma once

struct rbtree_node {
    int key;
    int color;
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
    struct rbtree_node* to_insert;
};

void rbtree_init(struct rbtree* tree);
struct rbtree_find_result rbtree_find(struct rbtree* tree, int key);
struct rbtree_node* rbtree_first(struct rbtree* tree);
struct rbtree_node* rbtree_next(struct rbtree_node* node);
void rbtree_insert(struct rbtree* tree, struct rbtree_node* node);
void rbtree_remove(struct rbtree* tree, struct rbtree_node* node);
