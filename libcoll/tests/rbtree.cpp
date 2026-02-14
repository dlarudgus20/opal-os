#include <gtest/gtest.h>

extern "C" {
#define restrict
#include "collections/rbtree.h"
};

#include <iostream>
#include <climits>
#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <random>

enum { RED, BLK };

TEST(rbtree_test, empty_tree) {
    rbtree tree;
    rbtree_init(&tree);
    ASSERT_EQ(tree.root, nullptr);
}

TEST(rbtree_test, insert_single) {
    rbtree tree;
    rbtree_node node;
    node.key = 10;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node);
    ASSERT_EQ(tree.root, &node);
    ASSERT_EQ(node.key, 10);
    ASSERT_EQ(node.color, BLK);
}

TEST(rbtree_test, insert_multiple_no_rotation) {
    rbtree tree;
    rbtree_node node1;
    rbtree_node node2;
    rbtree_node node3;
    node1.key = 20;
    node2.key = 10;
    node3.key = 30;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);
    rbtree_insert(&tree, &node3);

    ASSERT_EQ(tree.root, &node1);
    ASSERT_EQ(node1.key, 20);
    ASSERT_EQ(node1.color, BLK);

    ASSERT_EQ(node1.left, &node2);
    ASSERT_EQ(node2.key, 10);
    ASSERT_EQ(node2.color, RED);

    ASSERT_EQ(node1.right, &node3);
    ASSERT_EQ(node3.key, 30);
    ASSERT_EQ(node3.color, RED);
}

TEST(rbtree_test, insert_multiple_left_rotation) {
    rbtree tree;
    rbtree_node node1;
    rbtree_node node2;
    rbtree_node node3;
    node1.key = 10;
    node2.key = 20;
    node3.key = 30;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);
    rbtree_insert(&tree, &node3);

    ASSERT_EQ(tree.root, &node2);
    ASSERT_EQ(node2.key, 20);
    ASSERT_EQ(node2.color, BLK);

    ASSERT_EQ(node2.left, &node1);
    ASSERT_EQ(node1.key, 10);
    ASSERT_EQ(node1.color, RED);

    ASSERT_EQ(node2.right, &node3);
    ASSERT_EQ(node3.key, 30);
    ASSERT_EQ(node3.color, RED);
}

TEST(rbtree_test, insert_multiple_right_rotation) {
    rbtree tree;
    rbtree_node node1;
    rbtree_node node2;
    rbtree_node node3;
    node1.key = 30;
    node2.key = 20;
    node3.key = 10;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);
    rbtree_insert(&tree, &node3);

    ASSERT_EQ(tree.root, &node2);
    ASSERT_EQ(node2.key, 20);
    ASSERT_EQ(node2.color, BLK);

    ASSERT_EQ(node2.left, &node3);
    ASSERT_EQ(node3.key, 10);
    ASSERT_EQ(node3.color, RED);

    ASSERT_EQ(node2.right, &node1);
    ASSERT_EQ(node1.key, 30);
    ASSERT_EQ(node1.color, RED);
}

TEST(rbtree_test, insert_multiple_left_right_rotation) {
    rbtree tree;
    rbtree_node node1;
    rbtree_node node2;
    rbtree_node node3;
    node1.key = 30;
    node2.key = 10;
    node3.key = 20;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);
    rbtree_insert(&tree, &node3);

    ASSERT_EQ(tree.root, &node3);
    ASSERT_EQ(node3.key, 20);
    ASSERT_EQ(node3.color, BLK);

    ASSERT_EQ(node3.left, &node2);
    ASSERT_EQ(node2.key, 10);
    ASSERT_EQ(node2.color, RED);

    ASSERT_EQ(node3.right, &node1);
    ASSERT_EQ(node1.key, 30);
    ASSERT_EQ(node1.color, RED);
}

TEST(rbtree_test, insert_multiple_right_left_rotation) {
    rbtree tree;
    rbtree_node node1;
    rbtree_node node2;
    rbtree_node node3;
    node1.key = 10;
    node2.key = 30;
    node3.key = 20;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);
    rbtree_insert(&tree, &node3);

    ASSERT_EQ(tree.root, &node3);
    ASSERT_EQ(node3.key, 20);
    ASSERT_EQ(node3.color, BLK);

    ASSERT_EQ(node3.left, &node1);
    ASSERT_EQ(node1.key, 10);
    ASSERT_EQ(node1.color, RED);

    ASSERT_EQ(node3.right, &node2);
    ASSERT_EQ(node2.key, 30);
    ASSERT_EQ(node2.color, RED);
}

TEST(rbtree_test, insert_and_find) {
    rbtree tree;
    rbtree_node node1, node2, node3;
    node1.key = 5;
    node2.key = 15;
    node3.key = 10;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);
    rbtree_insert(&tree, &node3);

    rbtree_find_result found = rbtree_find(&tree, 10);
    ASSERT_EQ(found.lower, &node3);
    ASSERT_EQ(found.upper, &node3);
    ASSERT_EQ(found.to_insert, nullptr);

    found = rbtree_find(&tree, 15);
    ASSERT_EQ(found.lower, &node2);
    ASSERT_EQ(found.upper, &node2);
    ASSERT_EQ(found.to_insert, nullptr);

    found = rbtree_find(&tree, 5);
    ASSERT_EQ(found.lower, &node1);
    ASSERT_EQ(found.upper, &node1);
    ASSERT_EQ(found.to_insert, nullptr);

    found = rbtree_find(&tree, 100);
    ASSERT_EQ(found.lower, &node2);
    ASSERT_EQ(found.upper, nullptr);
    ASSERT_EQ(found.to_insert, &node2);
}

TEST(rbtree_test, insert_duplicate_key) {
    rbtree tree;
    rbtree_node node1, node2;
    node1.key = 42;
    node2.key = 42;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);

    ASSERT_EQ(tree.root, &node1);
    ASSERT_EQ(node1.left, nullptr);
    ASSERT_EQ(node1.right, nullptr);
    ASSERT_EQ(node1.color, BLK);
}

TEST(rbtree_test, remove_leaf_node) {
    rbtree tree;
    rbtree_node node1, node2, node3;
    node1.key = 20;
    node2.key = 10;
    node3.key = 30;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);
    rbtree_insert(&tree, &node3);

    rbtree_remove(&tree, &node3);
    ASSERT_EQ(node1.right, nullptr);

    rbtree_find_result found = rbtree_find(&tree, 30);
    ASSERT_EQ(found.lower, &node1);
    ASSERT_EQ(found.upper, nullptr);
    ASSERT_EQ(found.to_insert, &node1);
}

TEST(rbtree_test, remove_root_node) {
    rbtree tree;
    rbtree_node node1, node2;
    node1.key = 50;
    node2.key = 25;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);

    rbtree_remove(&tree, &node1);
    ASSERT_EQ(tree.root, &node2);

    rbtree_find_result found = rbtree_find(&tree, 50);
    ASSERT_EQ(found.lower, &node2);
    ASSERT_EQ(found.upper, nullptr);
    ASSERT_EQ(found.to_insert, &node2);
}

TEST(rbtree_test, remove_node_with_one_child) {
    rbtree tree;
    rbtree_node node1, node2, node3;
    node1.key = 100;
    node2.key = 50;
    node3.key = 75;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);
    rbtree_insert(&tree, &node3);
    ASSERT_EQ(tree.root, &node3);

    rbtree_remove(&tree, &node2);
    ASSERT_EQ(node3.left, nullptr);
    ASSERT_EQ(node3.right, &node1);

    rbtree_find_result found = rbtree_find(&tree, 50);
    ASSERT_EQ(found.lower, nullptr);
    ASSERT_EQ(found.upper, &node3);
    ASSERT_EQ(found.to_insert, &node3);
}

TEST(rbtree_test, remove_node_with_two_children) {
    rbtree tree;
    rbtree_node node1, node2, node3, node4;
    node1.key = 40;
    node2.key = 20;
    node3.key = 60;
    node4.key = 30;
    rbtree_init(&tree);
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);
    rbtree_insert(&tree, &node3);
    rbtree_insert(&tree, &node4);

    rbtree_remove(&tree, &node2);
    ASSERT_EQ(node1.left, &node4);

    rbtree_find_result found = rbtree_find(&tree, 20);
    ASSERT_EQ(found.lower, nullptr);
    ASSERT_EQ(found.upper, &node4);
    ASSERT_EQ(found.to_insert, &node4);
}

TEST(rbtree_test, inorder_traversal) {
    rbtree tree;
    rbtree_node nodes[5];
    int keys[5] = {40, 20, 60, 10, 30};
    for (int i = 0; i < 5; ++i) nodes[i].key = keys[i];
    rbtree_init(&tree);
    for (int i = 0; i < 5; ++i) rbtree_insert(&tree, &nodes[i]);

    int result[5], idx = 0;
    auto inorder = [&](auto self, rbtree_node *node) {
        if (!node) return;
        self(self, node->left);
        result[idx++] = node->key;
        self(self, node->right);
    };
    inorder(inorder, tree.root);

    ASSERT_EQ(result[0], 10);
    ASSERT_EQ(result[1], 20);
    ASSERT_EQ(result[2], 30);
    ASSERT_EQ(result[3], 40);
    ASSERT_EQ(result[4], 60);
}

TEST(rbtree_test, traversal_100) {
    constexpr int size = 100;

    rbtree tree;
    rbtree_node nodes[size];
    rbtree_init(&tree);
    for (int i = 0; i < size; ++i) {
        nodes[i].key = i;
        rbtree_insert(&tree, &nodes[i]);
    }

    int i = 0;
    for (rbtree_node* node = rbtree_first(&tree); node != nullptr; node = rbtree_next(node)) {
        ASSERT_EQ(node->key, i);
        i += 1;
    }
}

TEST(rbtree_test, stress_insert_and_remove) {
    rbtree tree;
    rbtree_node nodes[100];
    rbtree_init(&tree);
    for (int i = 0; i < 100; ++i) {
        nodes[i].key = i;
        rbtree_insert(&tree, &nodes[i]);
        ASSERT_EQ(tree.root->parent, nullptr);
    }
    for (int i = 0; i < 100; ++i) {
        struct rbtree_find_result r = rbtree_find(&tree, i);
        ASSERT_EQ(r.lower, &nodes[i]);
        ASSERT_EQ(r.upper, &nodes[i]);
        ASSERT_EQ(r.to_insert, nullptr);
    }
    for (int i = 0; i < 100; ++i) {
        ASSERT_EQ(tree.root->parent, nullptr);
        rbtree_remove(&tree, &nodes[i]);
        struct rbtree_find_result r = rbtree_find(&tree, i);
        ASSERT_TRUE(!r.lower || r.lower->key != i);
        ASSERT_TRUE(!r.upper || r.upper->key != i);
    }
    ASSERT_EQ(tree.root, nullptr);
}

TEST(rbtree_test, insert_min_max_keys) {
    rbtree tree;
    rbtree_node min_node, max_node;
    rbtree_init(&tree);
    min_node.key = INT_MIN;
    max_node.key = INT_MAX;
    rbtree_insert(&tree, &min_node);
    rbtree_insert(&tree, &max_node);
    auto min_res = rbtree_find(&tree, INT_MIN);
    auto max_res = rbtree_find(&tree, INT_MAX);
    ASSERT_EQ(min_res.lower, &min_node);
    ASSERT_EQ(max_res.lower, &max_node);
}

TEST(rbtree_test, tree_properties_after_insert) {
    rbtree tree;
    constexpr int N = 100;
    rbtree_node nodes[N];
    rbtree_init(&tree);
    for (int i = 0; i < N; ++i) {
        nodes[i].key = i;
        rbtree_insert(&tree, &nodes[i]);
    }
    // Check root is black
    ASSERT_EQ(tree.root->color, BLK);
    // Check no red node has red child
    std::function<void(rbtree_node*)> check_red = [&](rbtree_node* node) {
        if (!node) return;
        if (node->color == RED) {
            if (node->left) {
                ASSERT_EQ(node->left->color, BLK);
            }
            if (node->right) {
                ASSERT_EQ(node->right->color, BLK);
            }
        }
        check_red(node->left);
        check_red(node->right);
    };
    check_red(tree.root);
}

TEST(rbtree_test, tree_properties_after_remove) {
    rbtree tree;
    constexpr int N = 50;
    rbtree_node nodes[N];
    rbtree_init(&tree);
    for (int i = 0; i < N; ++i) {
        nodes[i].key = i;
        rbtree_insert(&tree, &nodes[i]);
    }
    for (int i = 0; i < N; i += 2) {
        rbtree_remove(&tree, &nodes[i]);
    }
    // Check root is black
    if (tree.root) {
        ASSERT_EQ(tree.root->color, BLK);
    }
}

TEST(rbtree_test, insert_remove_all_random_order) {
    std::random_device rd;
    auto seed = rd();
    std::cout << "seed is " << seed << std::endl;
    std::mt19937 rng(seed);

    rbtree tree;
    constexpr int N = 30;
    rbtree_node nodes[N];
    int keys[N];
    rbtree_init(&tree);
    for (int i = 0; i < N; ++i) keys[i] = i;
    std::shuffle(keys, keys + N, rng);
    for (int i = 0; i < N; ++i) {
        nodes[keys[i]].key = keys[i];
        rbtree_insert(&tree, &nodes[keys[i]]);
    }
    std::shuffle(keys, keys + N, rng);
    for (int i = 0; i < N; ++i) {
        rbtree_remove(&tree, &nodes[keys[i]]);
    }
    ASSERT_EQ(tree.root, nullptr);
}

TEST(rbtree_test, find_upper_lower) {
    rbtree tree;
    rbtree_node node1, node2, node3;
    rbtree_init(&tree);
    node1.key = 10;
    node2.key = 20;
    node3.key = 30;
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);
    rbtree_insert(&tree, &node3);
    auto res = rbtree_find(&tree, 25);
    ASSERT_EQ(res.lower, &node2);
    ASSERT_EQ(res.upper, &node3);
    ASSERT_EQ(res.to_insert, &node3);
}

TEST(rbtree_test, insert_remove_alternating_order) {
    rbtree tree;
    constexpr int N = 40;
    rbtree_node nodes[N];
    rbtree_init(&tree);
    for (int i = 0; i < N; ++i) {
        nodes[i].key = i;
        rbtree_insert(&tree, &nodes[i]);
        if (i % 3 == 2) {
            rbtree_remove(&tree, &nodes[i - 2]);
        }
    }
    int count = 0;
    for (rbtree_node* node = rbtree_first(&tree); node != nullptr; node = rbtree_next(node)) {
        ++count;
    }
    ASSERT_EQ(count, N - N / 3);
}

TEST(rbtree_test, insert_descending_keys) {
    rbtree tree;
    rbtree_node nodes[10];
    rbtree_init(&tree);
    for (int i = 9; i >= 0; --i) {
        nodes[i].key = i;
        rbtree_insert(&tree, &nodes[i]);
    }
    int expected = 0;
    for (rbtree_node* node = rbtree_first(&tree); node; node = rbtree_next(node)) {
        ASSERT_EQ(node->key, expected++);
    }
}

TEST(rbtree_test, insert_ascending_keys) {
    rbtree tree;
    rbtree_node nodes[10];
    rbtree_init(&tree);
    for (int i = 0; i < 10; ++i) {
        nodes[i].key = i;
        rbtree_insert(&tree, &nodes[i]);
    }
    int expected = 0;
    for (rbtree_node* node = rbtree_first(&tree); node; node = rbtree_next(node)) {
        ASSERT_EQ(node->key, expected++);
    }
}

TEST(rbtree_test, remove_all_nodes) {
    rbtree tree;
    rbtree_node nodes[10];
    rbtree_init(&tree);
    for (int i = 0; i < 10; ++i) {
        nodes[i].key = i;
        rbtree_insert(&tree, &nodes[i]);
    }
    for (int i = 0; i < 10; ++i) {
        rbtree_remove(&tree, &nodes[i]);
    }
    ASSERT_EQ(tree.root, nullptr);
}

TEST(rbtree_test, find_nonexistent_key) {
    rbtree tree;
    rbtree_node node1, node2;
    rbtree_init(&tree);
    node1.key = 1;
    node2.key = 2;
    rbtree_insert(&tree, &node1);
    rbtree_insert(&tree, &node2);
    auto res = rbtree_find(&tree, 3);
    ASSERT_EQ(res.lower, &node2);
    ASSERT_EQ(res.upper, nullptr);
    ASSERT_EQ(res.to_insert, &node2);
}

TEST(rbtree_test, insert_remove_min_max) {
    rbtree tree;
    rbtree_node min_node, max_node;
    rbtree_init(&tree);
    min_node.key = INT_MIN;
    max_node.key = INT_MAX;
    rbtree_insert(&tree, &min_node);
    rbtree_insert(&tree, &max_node);
    rbtree_remove(&tree, &min_node);
    auto res = rbtree_find(&tree, INT_MIN);
    ASSERT_EQ(res.lower, nullptr);
    ASSERT_EQ(res.upper, &max_node);
    rbtree_remove(&tree, &max_node);
    ASSERT_EQ(tree.root, nullptr);
}

TEST(rbtree_test, insert_duplicate_keys_multiple) {
    rbtree tree;
    rbtree_node nodes[5];
    rbtree_init(&tree);
    for (int i = 0; i < 5; ++i) {
        nodes[i].key = 42;
        rbtree_insert(&tree, &nodes[i]);
    }
    ASSERT_EQ(tree.root->key, 42);
    ASSERT_EQ(tree.root->left, nullptr);
    ASSERT_EQ(tree.root->right, nullptr);
}

TEST(rbtree_test, insert_and_remove_alternating) {
    rbtree tree;
    rbtree_node nodes[20];
    rbtree_init(&tree);
    for (int i = 0; i < 20; ++i) {
        nodes[i].key = i;
        rbtree_insert(&tree, &nodes[i]);
        if (i % 2 == 1) {
            rbtree_remove(&tree, &nodes[i - 1]);
        }
    }
    int count = 0;
    for (rbtree_node* node = rbtree_first(&tree); node; node = rbtree_next(node)) {
        ++count;
    }
    ASSERT_EQ(count, 10);
}
