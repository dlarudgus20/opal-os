#include <stdint.h>

#include <opal/test.h>
#include <opal/utils/vmtree.h>

[[maybe_unused]] static void expect_entry(struct vmtree *tree, uintptr_t addr, uintptr_t start, uintptr_t end, void *entry) {
    struct vmtree_entry got = vmtree_get(tree, addr);
    TEST_EXPECT_EQ(start, got.start);
    TEST_EXPECT_EQ(end, got.end);
    TEST_EXPECT_EQ((uint64_t)(uintptr_t)entry, (uint64_t)(uintptr_t)got.entry);
}

DEFINE_UNIT_TEST(vmtree_init_is_single_hole) {
    struct vmtree tree;
    vmtree_init(&tree);

    struct vmtree_entry e = vmtree_get(&tree, 1234);
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)e.start);
    TEST_EXPECT_EQ((uint64_t)UINTPTR_MAX, (uint64_t)e.end);
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)e.entry);
}

DEFINE_UNIT_TEST(vmtree_insert_and_merge_neighbors) {
    struct vmtree tree;
    vmtree_init(&tree);

    void *a = (void *)(uintptr_t)0x1000;
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_insert(&tree, 10, 20, a));
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_insert(&tree, 20, 30, a));

    expect_entry(&tree, 9, 0, 10, NULL);
    expect_entry(&tree, 10, 10, 30, a);
    expect_entry(&tree, 29, 10, 30, a);
    expect_entry(&tree, 30, 30, UINTPTR_MAX, NULL);
}

DEFINE_UNIT_TEST(vmtree_insert_rejects_overlap) {
    struct vmtree tree;
    vmtree_init(&tree);

    void *a = (void *)(uintptr_t)0x2000;
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_insert(&tree, 100, 200, a));
    TEST_EXPECT_EQ(VMTREE_ERR_EXISTS, vmtree_insert(&tree, 150, 160, (void *)(uintptr_t)0x3000));
    TEST_EXPECT_EQ(VMTREE_ERR_EXISTS, vmtree_insert(&tree, 50, 120, (void *)(uintptr_t)0x3000));
}

DEFINE_UNIT_TEST(vmtree_set_requires_non_hole) {
    struct vmtree tree;
    vmtree_init(&tree);

    void *a = (void *)(uintptr_t)0x3000;
    void *b = (void *)(uintptr_t)0x4000;
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_insert(&tree, 1000, 2000, a));

    TEST_EXPECT_EQ(VMTREE_ERR_NOENT, vmtree_set(&tree, 900, 1100, b));
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_set(&tree, 1200, 1800, b));

    expect_entry(&tree, 1100, 1000, 1200, a);
    expect_entry(&tree, 1500, 1200, 1800, b);
    expect_entry(&tree, 1900, 1800, 2000, a);
}

DEFINE_UNIT_TEST(vmtree_remove_is_idempotent_and_merges_holes) {
    struct vmtree tree;
    vmtree_init(&tree);

    void *a = (void *)(uintptr_t)0x5000;
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_insert(&tree, 64, 128, a));
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_remove(&tree, 80, 96));
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_remove(&tree, 80, 96));
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_remove(&tree, 64, 128));

    struct vmtree_entry e = vmtree_get(&tree, 70);
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)e.start);
    TEST_EXPECT_EQ((uint64_t)UINTPTR_MAX, (uint64_t)e.end);
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)e.entry);
}

DEFINE_UNIT_TEST(vmtree_invalid_and_zero_length_contracts) {
    struct vmtree tree;
    vmtree_init(&tree);

    void *a = (void *)(uintptr_t)0x6000;
    TEST_EXPECT_EQ(VMTREE_ERR_INVAL, vmtree_insert(&tree, 20, 10, a));
    TEST_EXPECT_EQ(VMTREE_ERR_INVAL, vmtree_insert(&tree, 10, 20, NULL));
    TEST_EXPECT_EQ(VMTREE_ERR_INVAL, vmtree_set(&tree, 20, 10, a));
    TEST_EXPECT_EQ(VMTREE_ERR_INVAL, vmtree_set(&tree, 10, 20, NULL));
    TEST_EXPECT_EQ(VMTREE_ERR_INVAL, vmtree_remove(&tree, 20, 10));

    TEST_EXPECT_EQ(VMTREE_OK, vmtree_insert(&tree, 10, 10, a));
    TEST_EXPECT_EQ(VMTREE_OK, vmtree_set(&tree, 10, 10, a));
    TEST_EXPECT_EQ(VMTREE_OK, vmtree_remove(&tree, 10, 10));
}

DEFINE_UNIT_TEST(vmtree_iter_empty_tree) {
    struct vmtree tree;
    vmtree_init(&tree);

    struct vmtree_iter iter = vmtree_before_begin(&tree);
    struct vmtree_entry entry;
    TEST_EXPECT_FALSE(vmtree_iter_next(&iter, &entry));
}

DEFINE_UNIT_TEST(vmtree_iter_returns_non_hole_in_sorted_order) {
    struct vmtree tree;
    vmtree_init(&tree);

    void *a = (void *)(uintptr_t)0x1110;
    void *b = (void *)(uintptr_t)0x2220;
    void *c = (void *)(uintptr_t)0x3330;
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_insert(&tree, 100, 200, a));
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_insert(&tree, 400, 500, b));
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_insert(&tree, 250, 260, c));

    struct vmtree_iter iter = vmtree_before_begin(&tree);
    struct vmtree_entry entry;

    TEST_ASSERT_TRUE(vmtree_iter_next(&iter, &entry));
    TEST_EXPECT_EQ((uint64_t)100, (uint64_t)entry.start);
    TEST_EXPECT_EQ((uint64_t)200, (uint64_t)entry.end);
    TEST_EXPECT_EQ((uint64_t)(uintptr_t)a, (uint64_t)(uintptr_t)entry.entry);

    TEST_ASSERT_TRUE(vmtree_iter_next(&iter, &entry));
    TEST_EXPECT_EQ((uint64_t)250, (uint64_t)entry.start);
    TEST_EXPECT_EQ((uint64_t)260, (uint64_t)entry.end);
    TEST_EXPECT_EQ((uint64_t)(uintptr_t)c, (uint64_t)(uintptr_t)entry.entry);

    TEST_ASSERT_TRUE(vmtree_iter_next(&iter, &entry));
    TEST_EXPECT_EQ((uint64_t)400, (uint64_t)entry.start);
    TEST_EXPECT_EQ((uint64_t)500, (uint64_t)entry.end);
    TEST_EXPECT_EQ((uint64_t)(uintptr_t)b, (uint64_t)(uintptr_t)entry.entry);

    TEST_EXPECT_FALSE(vmtree_iter_next(&iter, &entry));
}

DEFINE_UNIT_TEST(vmtree_iter_skips_holes_after_remove) {
    struct vmtree tree;
    vmtree_init(&tree);

    void *a = (void *)(uintptr_t)0x4440;
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_insert(&tree, 100, 200, a));
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_remove(&tree, 130, 170));

    struct vmtree_iter iter = vmtree_before_begin(&tree);
    struct vmtree_entry entry;

    TEST_ASSERT_TRUE(vmtree_iter_next(&iter, &entry));
    TEST_EXPECT_EQ((uint64_t)100, (uint64_t)entry.start);
    TEST_EXPECT_EQ((uint64_t)130, (uint64_t)entry.end);
    TEST_EXPECT_EQ((uint64_t)(uintptr_t)a, (uint64_t)(uintptr_t)entry.entry);

    TEST_ASSERT_TRUE(vmtree_iter_next(&iter, &entry));
    TEST_EXPECT_EQ((uint64_t)170, (uint64_t)entry.start);
    TEST_EXPECT_EQ((uint64_t)200, (uint64_t)entry.end);
    TEST_EXPECT_EQ((uint64_t)(uintptr_t)a, (uint64_t)(uintptr_t)entry.entry);

    TEST_EXPECT_FALSE(vmtree_iter_next(&iter, &entry));
}

DEFINE_UNIT_TEST(vmtree_iter_walks_non_hole_ranges) {
    struct vmtree tree;
    vmtree_init(&tree);

    void *a = (void *)(uintptr_t)0x5550;
    void *b = (void *)(uintptr_t)0x6660;
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_insert(&tree, 8, 16, a));
    TEST_ASSERT_EQ(VMTREE_OK, vmtree_insert(&tree, 32, 40, b));

    uint64_t count = 0;
    uintptr_t starts_sum = 0;
    struct vmtree_iter iter = vmtree_before_begin(&tree);
    for (struct vmtree_entry entry; vmtree_iter_next(&iter, &entry); ) {
        count++;
        starts_sum += entry.start;
    }

    TEST_EXPECT_EQ((uint64_t)2, count);
    TEST_EXPECT_EQ((uint64_t)(8 + 32), (uint64_t)starts_sum);
}
