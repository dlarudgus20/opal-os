#include <opal/test.h>
#include <opal/utils/rbtree.h>

struct test_node {
    int key;
    struct rbtree_node node;
};

static int compare_key(const struct test_node *node, int key) {
    return (node->key > key) - (node->key < key);
}

static int compare_nodes(const struct test_node *a, const struct test_node *b) {
    return compare_key(a, b->key);
}

RBTREE_TEMPLATE(struct test_node, int, node, compare_nodes, compare_key, test, static)

DEFINE_TEST(utils_rbtree_find_bounds) {
    struct rbtree tree;
    struct test_node low = { .key = 10 };
    struct test_node mid = { .key = 20 };
    struct test_node high = { .key = 30 };
    rbtree_init(&tree);
    rbtree_insert_test(&tree, &low);
    rbtree_insert_test(&tree, &mid);
    rbtree_insert_test(&tree, &high);

    struct rbtree_find_result bounds = rbtree_find_test(&tree, 25);
    TEST_EXPECT_EQ(&mid.node, bounds.lower);
    TEST_EXPECT_EQ(&high.node, bounds.upper);

    bounds = rbtree_find_test(&tree, 20);
    TEST_EXPECT_EQ(&mid.node, bounds.lower);
    TEST_EXPECT_EQ(&mid.node, bounds.upper);
}

DEFINE_TEST(utils_rbtree_remove_minimum) {
    struct rbtree tree;
    struct test_node nodes[] = { { .key = 10 }, { .key = 20 }, { .key = 30 }, { .key = 40 } };
    rbtree_init(&tree);
    rbtree_insert_test(&tree, &nodes[3]);
    rbtree_insert_test(&tree, &nodes[0]);
    rbtree_insert_test(&tree, &nodes[2]);
    rbtree_insert_test(&tree, &nodes[1]);

    for (size_t i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++) {
        TEST_ASSERT_EQ(&nodes[i].node, rbtree_first(&tree));
        rbtree_remove(&tree, &nodes[i].node);
    }
    TEST_EXPECT_EQ(NULL, rbtree_first(&tree));
}
