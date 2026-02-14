#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <kc/assert.h>

#include "collections/rbtree.h"

enum { RED, BLK };

static struct rbtree_node* get_min_node(struct rbtree_node* node);
static void rotate_left(struct rbtree* tree, struct rbtree_node* node);
static void rotate_right(struct rbtree* tree, struct rbtree_node* node);

static void insertion_balancing(struct rbtree* tree, struct rbtree_node* node);
static void removal_balancing(struct rbtree* tree, struct rbtree_node* sibling);

static bool is_blk_or_nil(struct rbtree_node* node) {
    return node == NULL || node->color == BLK;
}

static bool is_red(struct rbtree_node* node) {
    return node != NULL && node->color == RED;
}

static struct rbtree_node* get_sibling(struct rbtree_node* node) {
    struct rbtree_node* parent = node->parent;
    if (parent == NULL) {
        return NULL;
    }
    return node == parent->left ? parent->right : parent->left;
}

void rbtree_init(struct rbtree* tree) {
    tree->root = NULL;
}

struct rbtree_find_result rbtree_find(struct rbtree* tree, int key) {
    struct rbtree_find_result result = { .lower = NULL, .upper = NULL, .to_insert = NULL };
    struct rbtree_node* next = tree->root;
    struct rbtree_node* node = NULL;
    while (1) {
        if (next == NULL) {
            result.to_insert = node;
            return result;
        } else {
            node = next;
        }

        if (node->key < key) {
            result.lower = node;
            next = node->right;
        } else if (node->key > key) {
            result.upper = node;
            next = node->left;
        } else {
            result.lower = result.upper = node;
            return result;
        }
    }
}

struct rbtree_node* rbtree_first(struct rbtree* tree) {
    return tree->root == NULL ? NULL : get_min_node(tree->root);
}

struct rbtree_node* rbtree_next(struct rbtree_node* node) {
    if (node->right != NULL) {
        return get_min_node(node->right);
    }

    while (1) {
        struct rbtree_node* parent = node->parent;
        if (parent == NULL) {
            return NULL;
        } else if (parent->left == node) {
            return parent;
        } else {
            node = parent;
        }
    }
}

void rbtree_insert(struct rbtree* tree, struct rbtree_node* node) {
    if (tree->root == NULL) {
        node->parent = NULL;
        node->left = node->right = NULL;
        node->color = BLK;
        tree->root = node;
    } else {
        struct rbtree_find_result r = rbtree_find(tree, node->key);
        if (r.to_insert == NULL) {
            return;
        } else {
            node->parent = r.to_insert;
            node->left = node->right = NULL;
            if (node->key < r.to_insert->key) {
                r.to_insert->left = node;
            } else {
                r.to_insert->right = node;
            }
            insertion_balancing(tree, node);
        }
    }
}

static void insertion_balancing(struct rbtree* tree, struct rbtree_node* node) {
    // rebalance tree for insertion

    struct rbtree_node* parent = node->parent;
    struct rbtree_node* grand;
    struct rbtree_node* uncle;

    // case 1: root
    if (parent == NULL) {
        node->color = BLK;
        return;
    }

    // case 2: parent is black
    node->color = RED;
    if (parent->color == BLK) {
        return;
    }

    // case 3: both parent and uncle are red
    grand = parent->parent;
    uncle = get_sibling(parent);
    if (uncle != NULL && uncle->color == RED) {
        parent->color = BLK;
        uncle->color = BLK;
        grand->color = RED;
        insertion_balancing(tree, grand);
        return;
    }

    // case 4: rotation for parent
    bool rotated = false;
    if (parent->right == node && grand->left == parent) {
        rotate_left(tree, parent);
        rotated = true;
    } else if (parent->left == node && grand->right == parent) {
        rotate_right(tree, parent);
        rotated = true;
    }
    if (rotated) {
        node = parent;
        parent = node->parent;
        grand = parent->parent;
    }

    // case 5: rotation for grand
    parent->color = BLK;
    grand->color = RED;
    if (parent->left == node) {
        rotate_right(tree, grand);
    } else {
        rotate_left(tree, grand);
    }
}

void rbtree_remove(struct rbtree* tree, struct rbtree_node* node) {
    if (node->left != NULL && node->right != NULL) {
        // case 0: node has two non-null children
        struct rbtree_node* successor = get_min_node(node->right);
        rbtree_remove(tree, successor);

        struct rbtree_node* parent = node->parent;
        if (parent != NULL) {
            if (parent->left == node) {
                parent->left = successor;
            } else {
                parent->right = successor;
            }
        }
        successor->parent = parent;
        successor->left = node->left;
        successor->right = node->right;
        successor->color = node->color;

        if (successor->left != NULL) {
            successor->left->parent = successor;
        }
        if (successor->right != NULL) {
            successor->right->parent = successor;
        }

        if (tree->root == node) {
            tree->root = successor;
        }
    } else {
        // replace node with its child
        struct rbtree_node* parent = node->parent;
        struct rbtree_node* child = node->left != NULL ? node->left : node->right;
        struct rbtree_node* sibling = NULL;
        if (parent != NULL) {
            if (parent->left == node) {
                parent->left = child;
                sibling = parent->right;
            } else {
                parent->right = child;
                sibling = parent->left;
            }
        } else {
            tree->root = child;
        }

        // case 0: node has one child
        if (child != NULL) {
            child->parent = parent;
            child->color = BLK;
            return;
        }

        // case 0: node is red
        if (node->color == RED) {
            return;
        }

        removal_balancing(tree, sibling);
    }
}

static void removal_balancing(struct rbtree* tree, struct rbtree_node* sibling) {
    // rebalance tree for removal of black node

    // non-null black node's sibiling cannot be null
    // thus sibling == NULL only if node is root
    // case 1: root
    if (sibling == NULL) {
        return;
    }

    struct rbtree_node* parent = sibling->parent;

    // case 2: sibling is red
    // then sibiling's children also cannot be null
    if (sibling->color == RED) {
        sibling->color = BLK;
        parent->color = RED;
        if (sibling == parent->right) {
            sibling = sibling->left;
            rotate_left(tree, parent);
        } else {
            sibling = sibling->right;
            rotate_right(tree, parent);
        }
    }

    // case 3: parent, sibling and its children are black
    if (parent->color == BLK
        && (sibling->left == NULL || sibling->left->color == BLK)
        && (sibling->right == NULL || sibling->right->color == BLK)
    ) {
        sibling->color = RED;
        removal_balancing(tree, get_sibling(parent));
        return;
    }

    // case 4: parent is red but sibling and its children are black
    if (parent->color == RED
        && (sibling->left == NULL || sibling->left->color == BLK)
        && (sibling->right == NULL || sibling->right->color == BLK)
    ) {
        sibling->color = RED;
        parent->color = BLK;
        return;
    }

    // case 5: sibling rotation
    if (sibling == parent->right
        && is_red(sibling->left) && is_blk_or_nil(sibling->right)
    ) {
        sibling->color = RED;
        sibling->left->color = BLK;

        rotate_right(tree, sibling);
        sibling = sibling->parent;
    } else if (sibling == parent->left
        && is_red(sibling->right) && is_blk_or_nil(sibling->left)
    ) {
        sibling->color = RED;
        sibling->right->color = BLK;

        rotate_left(tree, sibling);
        sibling = sibling->parent;
    }

    // case 6: parent rotation
    sibling->color = parent->color;
    parent->color = BLK;

    if (sibling == parent->right) {
        sibling->right->color = BLK;
        rotate_left(tree, parent);
    } else {
        sibling->left->color = BLK;
        rotate_right(tree, parent);
    }
}

static struct rbtree_node* get_min_node(struct rbtree_node* node) {
    while (1) {
        if (node->left == NULL) {
            return node;
        }
        node = node->left;
    }
}

static void rotate_left(struct rbtree* tree, struct rbtree_node* node) {
    struct rbtree_node* parent = node->parent;
    struct rbtree_node* right = node->right;

    assert(right != NULL, "rbtree: rotate_left with null right child");
    if (right->left != NULL) {
        right->left->parent = node;
    }
    node->right = right->left;
    node->parent = right;

    right->left = node;
    right->parent = parent;

    if (parent != NULL) {
        if (parent->left == node) {
            parent->left = right;
        } else {
            parent->right = right;
        }
    } else {
        tree->root = right;
    }
}

static void rotate_right(struct rbtree* tree, struct rbtree_node* node) {
    struct rbtree_node* parent = node->parent;
    struct rbtree_node* left = node->left;

    assert(left != NULL, "rbtree: rotate_right with null left child");
    if (left->right != NULL) {
        left->right->parent = node;
    }
    node->left = left->right;
    node->parent = left;

    left->right = node;
    left->parent = parent;

    if (parent != NULL) {
        if (parent->left == node) {
            parent->left = left;
        } else {
            parent->right = left;
        }
    } else {
        tree->root = left;
    }
}
