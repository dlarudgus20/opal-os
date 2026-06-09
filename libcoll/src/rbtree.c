#include <stddef.h>
#include <stdint.h>

#include <kc/kassert.h>

#include "collections/rbtree.h"

static struct rbtree_node *get_min_node(struct rbtree_node *node);
static void rotate_left(struct rbtree *tree, struct rbtree_node *node);
static void rotate_right(struct rbtree *tree, struct rbtree_node *node);

static void insertion_balancing(struct rbtree *tree, struct rbtree_node *node);
static void removal_balancing(struct rbtree *tree, struct rbtree_node *sibling);

static bool is_blk_or_nil(struct rbtree_node *node) {
    return node == NULL || node->color == RBTREE_BLACK;
}

static bool is_red(struct rbtree_node *node) {
    return node != NULL && node->color == RBTREE_RED;
}

static struct rbtree_node *get_sibling(struct rbtree_node *node) {
    struct rbtree_node *parent = node->parent;
    if (parent == NULL) {
        return NULL;
    }
    return node == parent->left ? parent->right : parent->left;
}

void rbtree_init(struct rbtree *tree) {
    tree->root = NULL;
    tree->first = NULL;
}

struct rbtree_node *rbtree_first(struct rbtree *tree) {
    return tree->first;
}

struct rbtree_node *rbtree_next(struct rbtree_node *node) {
    if (node->right != NULL) {
        return get_min_node(node->right);
    }

    while (1) {
        struct rbtree_node *parent = node->parent;
        if (parent == NULL) {
            return NULL;
        } else if (parent->left == node) {
            return parent;
        } else {
            node = parent;
        }
    }
}

void rbtree_link_insert(struct rbtree *tree, struct rbtree_node *parent, struct rbtree_node **link,
    struct rbtree_node *node) {
    if (tree->first == NULL || link == &tree->first->left) {
        tree->first = node;
    }
    node->parent = parent;
    node->left = node->right = NULL;
    *link = node;
    insertion_balancing(tree, node);
}

static void insertion_balancing(struct rbtree *tree, struct rbtree_node *node) {
    // rebalance tree for insertion

    struct rbtree_node *parent = node->parent;
    struct rbtree_node *grand;
    struct rbtree_node *uncle;

    // case 1: root
    if (parent == NULL) {
        node->color = RBTREE_BLACK;
        return;
    }

    // case 2: parent is black
    node->color = RBTREE_RED;
    if (parent->color == RBTREE_BLACK) {
        return;
    }

    // case 3: both parent and uncle are red
    grand = parent->parent;
    uncle = get_sibling(parent);
    if (uncle != NULL && uncle->color == RBTREE_RED) {
        parent->color = RBTREE_BLACK;
        uncle->color = RBTREE_BLACK;
        grand->color = RBTREE_RED;
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
    parent->color = RBTREE_BLACK;
    grand->color = RBTREE_RED;
    if (parent->left == node) {
        rotate_right(tree, grand);
    } else {
        rotate_left(tree, grand);
    }
}

void rbtree_remove(struct rbtree *tree, struct rbtree_node *node) {
    if (tree->first == node) {
        tree->first = rbtree_next(node);
    }

    if (node->left != NULL && node->right != NULL) {
        // case 0: node has two non-null children
        struct rbtree_node *successor = get_min_node(node->right);
        rbtree_remove(tree, successor);

        struct rbtree_node *parent = node->parent;
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
        struct rbtree_node *parent = node->parent;
        struct rbtree_node *child = node->left != NULL ? node->left : node->right;
        struct rbtree_node *sibling = NULL;
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
            child->color = RBTREE_BLACK;
            return;
        }

        // case 0: node is red
        if (node->color == RBTREE_RED) {
            return;
        }

        removal_balancing(tree, sibling);
    }
}

static void removal_balancing(struct rbtree *tree, struct rbtree_node *sibling) {
    // rebalance tree for removal of black node

    // non-null black node's sibiling cannot be null
    // thus sibling == NULL only if node is root
    // case 1: root
    if (sibling == NULL) {
        return;
    }

    struct rbtree_node *parent = sibling->parent;

    // case 2: sibling is red
    // then sibiling's children also cannot be null
    if (sibling->color == RBTREE_RED) {
        sibling->color = RBTREE_BLACK;
        parent->color = RBTREE_RED;
        if (sibling == parent->right) {
            sibling = sibling->left;
            rotate_left(tree, parent);
        } else {
            sibling = sibling->right;
            rotate_right(tree, parent);
        }
    }

    // case 3: parent, sibling and its children are black
    if (parent->color == RBTREE_BLACK
        && (sibling->left == NULL || sibling->left->color == RBTREE_BLACK)
        && (sibling->right == NULL || sibling->right->color == RBTREE_BLACK)) {
        sibling->color = RBTREE_RED;
        removal_balancing(tree, get_sibling(parent));
        return;
    }

    // case 4: parent is red but sibling and its children are black
    if (parent->color == RBTREE_RED
        && (sibling->left == NULL || sibling->left->color == RBTREE_BLACK)
        && (sibling->right == NULL || sibling->right->color == RBTREE_BLACK)) {
        sibling->color = RBTREE_RED;
        parent->color = RBTREE_BLACK;
        return;
    }

    // case 5: sibling rotation
    if (sibling == parent->right && is_red(sibling->left) && is_blk_or_nil(sibling->right)) {
        sibling->color = RBTREE_RED;
        sibling->left->color = RBTREE_BLACK;

        rotate_right(tree, sibling);
        sibling = sibling->parent;
    } else if (sibling == parent->left && is_red(sibling->right) && is_blk_or_nil(sibling->left)) {
        sibling->color = RBTREE_RED;
        sibling->right->color = RBTREE_BLACK;

        rotate_left(tree, sibling);
        sibling = sibling->parent;
    }

    // case 6: parent rotation
    sibling->color = parent->color;
    parent->color = RBTREE_BLACK;

    if (sibling == parent->right) {
        sibling->right->color = RBTREE_BLACK;
        rotate_left(tree, parent);
    } else {
        sibling->left->color = RBTREE_BLACK;
        rotate_right(tree, parent);
    }
}

static struct rbtree_node *get_min_node(struct rbtree_node *node) {
    while (1) {
        if (node->left == NULL) {
            return node;
        }
        node = node->left;
    }
}

static void rotate_left(struct rbtree *tree, struct rbtree_node *node) {
    struct rbtree_node *parent = node->parent;
    struct rbtree_node *right = node->right;

    kassert(right != NULL, "rbtree: rotate_left with null right child");
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

static void rotate_right(struct rbtree *tree, struct rbtree_node *node) {
    struct rbtree_node *parent = node->parent;
    struct rbtree_node *left = node->left;

    kassert(left != NULL, "rbtree: rotate_right with null left child");
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
