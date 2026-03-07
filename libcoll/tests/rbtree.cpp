#include "test-pch.h"

extern "C" {
#define restrict
#include <collections/rbtree.h>
};

struct mynode {
    int key;
    struct rbtree_node node;
};

static int comp_my_node(const mynode* a, const mynode* b) {
    if (a->key < b->key) {
        return -1;
    }
    if (a->key > b->key) {
        return 1;
    }
    return 0;
}

static int comp_my_key(const mynode* a, int key) {
    if (a->key < key) {
        return -1;
    }
    if (a->key > key) {
        return 1;
    }
    return 0;
}

RBTREE_TEMPLATE(mynode, int, node, comp_my_node, comp_my_key, my, static)

static mynode make_node(int key) {
    mynode node{};
    node.key = key;
    return node;
}

static mynode* as_data(struct rbtree_node* node) {
    return node == nullptr ? nullptr : container_of(node, mynode, node);
}

static mynode* first_data(struct rbtree* tree) {
    return as_data(rbtree_first(tree));
}

static mynode* next_data(mynode* data) {
    return data == nullptr ? nullptr : as_data(rbtree_next(&data->node));
}

static void assert_no_red_red(struct rbtree_node* node) {
    if (!node) {
        return;
    }

    if (node->color == RBTREE_RED) {
        if (node->left) {
            ASSERT_EQ(node->left->color, RBTREE_BLACK);
        }
        if (node->right) {
            ASSERT_EQ(node->right->color, RBTREE_BLACK);
        }
    }

    assert_no_red_red(node->left);
    assert_no_red_red(node->right);
}

TEST(rbtree_test, empty_tree) {
    rbtree tree;
    rbtree_init(&tree);
    ASSERT_EQ(tree.root, nullptr);
}

TEST(rbtree_test, insert_single) {
    rbtree tree;
    mynode node = make_node(10);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node);

    ASSERT_EQ(tree.root, &node.node);
    ASSERT_EQ(node.node.color, RBTREE_BLACK);
}

TEST(rbtree_test, insert_multiple_no_rotation) {
    rbtree tree;
    mynode node1 = make_node(20);
    mynode node2 = make_node(10);
    mynode node3 = make_node(30);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);
    rbtree_insert_my(&tree, &node3);

    ASSERT_EQ(tree.root, &node1.node);
    ASSERT_EQ(node1.node.color, RBTREE_BLACK);
    ASSERT_EQ(node1.node.left, &node2.node);
    ASSERT_EQ(node2.node.color, RBTREE_RED);
    ASSERT_EQ(node1.node.right, &node3.node);
    ASSERT_EQ(node3.node.color, RBTREE_RED);
}

TEST(rbtree_test, insert_multiple_left_rotation) {
    rbtree tree;
    mynode node1 = make_node(10);
    mynode node2 = make_node(20);
    mynode node3 = make_node(30);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);
    rbtree_insert_my(&tree, &node3);

    ASSERT_EQ(tree.root, &node2.node);
    ASSERT_EQ(node2.node.color, RBTREE_BLACK);
    ASSERT_EQ(node2.node.left, &node1.node);
    ASSERT_EQ(node1.node.color, RBTREE_RED);
    ASSERT_EQ(node2.node.right, &node3.node);
    ASSERT_EQ(node3.node.color, RBTREE_RED);
}

TEST(rbtree_test, insert_multiple_right_rotation) {
    rbtree tree;
    mynode node1 = make_node(30);
    mynode node2 = make_node(20);
    mynode node3 = make_node(10);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);
    rbtree_insert_my(&tree, &node3);

    ASSERT_EQ(tree.root, &node2.node);
    ASSERT_EQ(node2.node.color, RBTREE_BLACK);
    ASSERT_EQ(node2.node.left, &node3.node);
    ASSERT_EQ(node3.node.color, RBTREE_RED);
    ASSERT_EQ(node2.node.right, &node1.node);
    ASSERT_EQ(node1.node.color, RBTREE_RED);
}

TEST(rbtree_test, insert_multiple_left_right_rotation) {
    rbtree tree;
    mynode node1 = make_node(30);
    mynode node2 = make_node(10);
    mynode node3 = make_node(20);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);
    rbtree_insert_my(&tree, &node3);

    ASSERT_EQ(tree.root, &node3.node);
    ASSERT_EQ(node3.node.color, RBTREE_BLACK);
    ASSERT_EQ(node3.node.left, &node2.node);
    ASSERT_EQ(node2.node.color, RBTREE_RED);
    ASSERT_EQ(node3.node.right, &node1.node);
    ASSERT_EQ(node1.node.color, RBTREE_RED);
}

TEST(rbtree_test, insert_multiple_right_left_rotation) {
    rbtree tree;
    mynode node1 = make_node(10);
    mynode node2 = make_node(30);
    mynode node3 = make_node(20);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);
    rbtree_insert_my(&tree, &node3);

    ASSERT_EQ(tree.root, &node3.node);
    ASSERT_EQ(node3.node.color, RBTREE_BLACK);
    ASSERT_EQ(node3.node.left, &node1.node);
    ASSERT_EQ(node1.node.color, RBTREE_RED);
    ASSERT_EQ(node3.node.right, &node2.node);
    ASSERT_EQ(node2.node.color, RBTREE_RED);
}

TEST(rbtree_test, insert_and_find) {
    rbtree tree;
    mynode node1 = make_node(5);
    mynode node2 = make_node(15);
    mynode node3 = make_node(10);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);
    rbtree_insert_my(&tree, &node3);

    rbtree_find_result found = rbtree_find_my(&tree, 10);
    ASSERT_EQ(found.lower, &node3.node);
    ASSERT_EQ(found.upper, &node3.node);

    found = rbtree_find_my(&tree, 100);
    ASSERT_EQ(found.lower, &node2.node);
    ASSERT_EQ(found.upper, nullptr);
}

TEST(rbtree_test, insert_duplicate_key) {
    rbtree tree;
    mynode node1 = make_node(42);
    mynode node2 = make_node(42);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);

    ASSERT_EQ(tree.root, &node1.node);
    ASSERT_EQ(node1.node.left, nullptr);
    ASSERT_EQ(node1.node.right, nullptr);
}

TEST(rbtree_test, remove_leaf_node) {
    rbtree tree;
    mynode node1 = make_node(20);
    mynode node2 = make_node(10);
    mynode node3 = make_node(30);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);
    rbtree_insert_my(&tree, &node3);

    rbtree_remove(&tree, &node3.node);
    ASSERT_EQ(node1.node.right, nullptr);

    rbtree_find_result found = rbtree_find_my(&tree, 30);
    ASSERT_EQ(found.lower, &node1.node);
    ASSERT_EQ(found.upper, nullptr);
}

TEST(rbtree_test, remove_root_node) {
    rbtree tree;
    mynode node1 = make_node(50);
    mynode node2 = make_node(25);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);

    rbtree_remove(&tree, &node1.node);
    ASSERT_EQ(tree.root, &node2.node);

    rbtree_find_result found = rbtree_find_my(&tree, 50);
    ASSERT_EQ(found.lower, &node2.node);
    ASSERT_EQ(found.upper, nullptr);
}

TEST(rbtree_test, remove_node_with_one_child) {
    rbtree tree;
    mynode node1 = make_node(100);
    mynode node2 = make_node(50);
    mynode node3 = make_node(75);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);
    rbtree_insert_my(&tree, &node3);

    ASSERT_EQ(tree.root, &node3.node);
    rbtree_remove(&tree, &node2.node);

    ASSERT_EQ(node3.node.left, nullptr);
    ASSERT_EQ(node3.node.right, &node1.node);

    rbtree_find_result found = rbtree_find_my(&tree, 50);
    ASSERT_EQ(found.lower, nullptr);
    ASSERT_EQ(found.upper, &node3.node);
}

TEST(rbtree_test, remove_node_with_two_children) {
    rbtree tree;
    mynode node1 = make_node(40);
    mynode node2 = make_node(20);
    mynode node3 = make_node(60);
    mynode node4 = make_node(30);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);
    rbtree_insert_my(&tree, &node3);
    rbtree_insert_my(&tree, &node4);

    rbtree_remove(&tree, &node2.node);
    ASSERT_EQ(node1.node.left, &node4.node);

    rbtree_find_result found = rbtree_find_my(&tree, 20);
    ASSERT_EQ(found.lower, nullptr);
    ASSERT_EQ(found.upper, &node4.node);
}

TEST(rbtree_test, inorder_traversal) {
    rbtree tree;
    mynode nodes[5];
    const int keys[5] = { 40, 20, 60, 10, 30 };

    rbtree_init(&tree);
    for (int i = 0; i < 5; ++i) {
        nodes[i].key = keys[i];
        rbtree_insert_my(&tree, &nodes[i]);
    }

    int result[5] = { 0 };
    int idx = 0;
    for (mynode* node = first_data(&tree); node != nullptr; node = next_data(node)) {
        result[idx++] = node->key;
    }

    ASSERT_EQ(result[0], 10);
    ASSERT_EQ(result[1], 20);
    ASSERT_EQ(result[2], 30);
    ASSERT_EQ(result[3], 40);
    ASSERT_EQ(result[4], 60);
}

TEST(rbtree_test, traversal_ascending) {
    rbtree tree;
    constexpr int kSize = 100;
    mynode nodes[kSize];

    rbtree_init(&tree);
    for (int i = 0; i < kSize; ++i) {
        nodes[i].key = i;
        rbtree_insert_my(&tree, &nodes[i]);
    }

    int expected = 0;
    for (mynode* node = first_data(&tree); node != nullptr; node = next_data(node)) {
        ASSERT_EQ(node->key, expected++);
    }
    ASSERT_EQ(expected, kSize);
}

TEST(rbtree_test, stress_insert_and_remove) {
    rbtree tree;
    constexpr int kSize = 100;
    mynode nodes[kSize];

    rbtree_init(&tree);
    for (int i = 0; i < kSize; ++i) {
        nodes[i].key = i;
        rbtree_insert_my(&tree, &nodes[i]);
    }

    for (int i = 0; i < kSize; ++i) {
        ASSERT_EQ(tree.root->parent, nullptr);
        rbtree_find_result found = rbtree_find_my(&tree, i);
        ASSERT_EQ(found.lower, &nodes[i].node);
        ASSERT_EQ(found.upper, &nodes[i].node);
    }

    for (int i = 0; i < kSize; ++i) {
        ASSERT_EQ(tree.root->parent, nullptr);
        rbtree_remove(&tree, &nodes[i].node);
        rbtree_find_result found = rbtree_find_my(&tree, i);
        ASSERT_TRUE(found.lower == nullptr || as_data(found.lower)->key != i);
        ASSERT_TRUE(found.upper == nullptr || as_data(found.upper)->key != i);
    }

    ASSERT_EQ(tree.root, nullptr);
}

TEST(rbtree_test, insert_min_max_keys) {
    rbtree tree;
    mynode min_node = make_node(INT_MIN);
    mynode max_node = make_node(INT_MAX);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &min_node);
    rbtree_insert_my(&tree, &max_node);

    const rbtree_find_result min_res = rbtree_find_my(&tree, INT_MIN);
    const rbtree_find_result max_res = rbtree_find_my(&tree, INT_MAX);
    ASSERT_EQ(min_res.lower, &min_node.node);
    ASSERT_EQ(max_res.lower, &max_node.node);
}

TEST(rbtree_test, tree_properties_after_insert) {
    rbtree tree;
    constexpr int kSize = 100;
    mynode nodes[kSize];

    rbtree_init(&tree);
    for (int i = 0; i < kSize; ++i) {
        nodes[i].key = i;
        rbtree_insert_my(&tree, &nodes[i]);
    }

    ASSERT_EQ(tree.root->color, RBTREE_BLACK);
    assert_no_red_red(tree.root);
}

TEST(rbtree_test, tree_properties_after_remove) {
    rbtree tree;
    constexpr int kSize = 50;
    mynode nodes[kSize];

    rbtree_init(&tree);
    for (int i = 0; i < kSize; ++i) {
        nodes[i].key = i;
        rbtree_insert_my(&tree, &nodes[i]);
    }
    for (int i = 0; i < kSize; i += 2) {
        rbtree_remove(&tree, &nodes[i].node);
    }

    if (tree.root) {
        ASSERT_EQ(tree.root->color, RBTREE_BLACK);
        assert_no_red_red(tree.root);
    }
}

TEST(rbtree_test, insert_remove_all_random_order) {
    std::mt19937 rng(0x5eedu);

    rbtree tree;
    constexpr int kSize = 30;
    mynode nodes[kSize];
    int keys[kSize];

    rbtree_init(&tree);
    for (int i = 0; i < kSize; ++i) {
        keys[i] = i;
        nodes[i].key = i;
    }

    std::shuffle(keys, keys + kSize, rng);
    for (int i = 0; i < kSize; ++i) {
        rbtree_insert_my(&tree, &nodes[keys[i]]);
    }

    std::shuffle(keys, keys + kSize, rng);
    for (int i = 0; i < kSize; ++i) {
        rbtree_remove(&tree, &nodes[keys[i]].node);
    }

    ASSERT_EQ(tree.root, nullptr);
}

TEST(rbtree_test, find_upper_lower) {
    rbtree tree;
    mynode node1 = make_node(10);
    mynode node2 = make_node(20);
    mynode node3 = make_node(30);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);
    rbtree_insert_my(&tree, &node3);

    const rbtree_find_result res = rbtree_find_my(&tree, 25);
    ASSERT_EQ(res.lower, &node2.node);
    ASSERT_EQ(res.upper, &node3.node);
}

TEST(rbtree_test, insert_remove_alternating_order) {
    rbtree tree;
    constexpr int kSize = 40;
    mynode nodes[kSize];

    rbtree_init(&tree);
    for (int i = 0; i < kSize; ++i) {
        nodes[i].key = i;
        rbtree_insert_my(&tree, &nodes[i]);
        if (i % 3 == 2) {
            rbtree_remove(&tree, &nodes[i - 2].node);
        }
    }

    int count = 0;
    for (mynode* node = first_data(&tree); node != nullptr; node = next_data(node)) {
        (void)node;
        ++count;
    }
    ASSERT_EQ(count, kSize - kSize / 3);
}

TEST(rbtree_test, insert_descending_keys) {
    rbtree tree;
    mynode nodes[10];

    rbtree_init(&tree);
    for (int i = 9; i >= 0; --i) {
        nodes[i].key = i;
        rbtree_insert_my(&tree, &nodes[i]);
    }

    int expected = 0;
    for (mynode* node = first_data(&tree); node != nullptr; node = next_data(node)) {
        ASSERT_EQ(node->key, expected++);
    }
}

TEST(rbtree_test, insert_ascending_keys) {
    rbtree tree;
    mynode nodes[10];

    rbtree_init(&tree);
    for (int i = 0; i < 10; ++i) {
        nodes[i].key = i;
        rbtree_insert_my(&tree, &nodes[i]);
    }

    int expected = 0;
    for (mynode* node = first_data(&tree); node != nullptr; node = next_data(node)) {
        ASSERT_EQ(node->key, expected++);
    }
}

TEST(rbtree_test, remove_all_nodes) {
    rbtree tree;
    mynode nodes[10];

    rbtree_init(&tree);
    for (int i = 0; i < 10; ++i) {
        nodes[i].key = i;
        rbtree_insert_my(&tree, &nodes[i]);
    }
    for (int i = 0; i < 10; ++i) {
        rbtree_remove(&tree, &nodes[i].node);
    }

    ASSERT_EQ(tree.root, nullptr);
}

TEST(rbtree_test, find_nonexistent_key) {
    rbtree tree;
    mynode node1 = make_node(1);
    mynode node2 = make_node(2);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &node1);
    rbtree_insert_my(&tree, &node2);

    const rbtree_find_result res = rbtree_find_my(&tree, 3);
    ASSERT_EQ(res.lower, &node2.node);
    ASSERT_EQ(res.upper, nullptr);
}

TEST(rbtree_test, insert_remove_min_max) {
    rbtree tree;
    mynode min_node = make_node(INT_MIN);
    mynode max_node = make_node(INT_MAX);

    rbtree_init(&tree);
    rbtree_insert_my(&tree, &min_node);
    rbtree_insert_my(&tree, &max_node);

    rbtree_remove(&tree, &min_node.node);
    const rbtree_find_result res = rbtree_find_my(&tree, INT_MIN);
    ASSERT_EQ(res.lower, nullptr);
    ASSERT_EQ(res.upper, &max_node.node);

    rbtree_remove(&tree, &max_node.node);
    ASSERT_EQ(tree.root, nullptr);
}

TEST(rbtree_test, insert_duplicate_keys_multiple) {
    rbtree tree;
    mynode nodes[5];

    rbtree_init(&tree);
    for (int i = 0; i < 5; ++i) {
        nodes[i].key = 42;
        rbtree_insert_my(&tree, &nodes[i]);
    }

    ASSERT_EQ(as_data(tree.root)->key, 42);
    ASSERT_EQ(tree.root->left, nullptr);
    ASSERT_EQ(tree.root->right, nullptr);
}

TEST(rbtree_test, insert_and_remove_alternating) {
    rbtree tree;
    mynode nodes[20];

    rbtree_init(&tree);
    for (int i = 0; i < 20; ++i) {
        nodes[i].key = i;
        rbtree_insert_my(&tree, &nodes[i]);
        if (i % 2 == 1) {
            rbtree_remove(&tree, &nodes[i - 1].node);
        }
    }

    int count = 0;
    for (mynode* node = first_data(&tree); node != nullptr; node = next_data(node)) {
        (void)node;
        ++count;
    }
    ASSERT_EQ(count, 10);
}

TEST(rbtree_test, root_black_after_many_insertions) {
    rbtree tree;
    constexpr int kSize = 100;
    mynode nodes[kSize];

    rbtree_init(&tree);
    for (int i = 0; i < kSize; ++i) {
        nodes[i].key = i;
        rbtree_insert_my(&tree, &nodes[i]);
    }

    ASSERT_NE(tree.root, nullptr);
    ASSERT_EQ(tree.root->color, RBTREE_BLACK);
}
