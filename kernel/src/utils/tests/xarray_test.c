#include <stdint.h>

#include <opal/test.h>
#include <opal/utils/xarray.h>

DEFINE_UNIT_TEST(xarray_set_range_basic) {
    struct xarray xa;
    xarray_init(&xa, 8);

    bool ok = xarray_set_range(&xa, 10, 4, (void *)(uintptr_t)0x1000);
    TEST_ASSERT_TRUE(ok);

    TEST_EXPECT_EQ((uint64_t)0x1000, (uint64_t)(uintptr_t)xarray_get(&xa, 10));
    TEST_EXPECT_EQ((uint64_t)0x1008, (uint64_t)(uintptr_t)xarray_get(&xa, 11));
    TEST_EXPECT_EQ((uint64_t)0x1010, (uint64_t)(uintptr_t)xarray_get(&xa, 12));
    TEST_EXPECT_EQ((uint64_t)0x1018, (uint64_t)(uintptr_t)xarray_get(&xa, 13));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, 14));
    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_set_range_overwrite) {
    struct xarray xa;
    xarray_init(&xa, 8);

    TEST_ASSERT_TRUE(xarray_set_range(&xa, 20, 3, (void *)(uintptr_t)0x2000));
    TEST_ASSERT_TRUE(xarray_set_range(&xa, 21, 2, (void *)(uintptr_t)0x3000));

    TEST_EXPECT_EQ((uint64_t)0x2000, (uint64_t)(uintptr_t)xarray_get(&xa, 20));
    TEST_EXPECT_EQ((uint64_t)0x3000, (uint64_t)(uintptr_t)xarray_get(&xa, 21));
    TEST_EXPECT_EQ((uint64_t)0x3008, (uint64_t)(uintptr_t)xarray_get(&xa, 22));
    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_set_range_zero_len) {
    struct xarray xa;
    xarray_init(&xa, 8);

    TEST_ASSERT_TRUE(xarray_set_range(&xa, 123, 0, (void *)(uintptr_t)0x5000));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, 123));
    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_set_range_high_index_expand) {
    struct xarray xa;
    xarray_init(&xa, 8);

    uint64_t high = (1ull << 18) + 7;
    TEST_ASSERT_TRUE(xarray_set_range(&xa, high, 2, (void *)(uintptr_t)0x7000));
    TEST_EXPECT_EQ((uint64_t)0x7000, (uint64_t)(uintptr_t)xarray_get(&xa, high));
    TEST_EXPECT_EQ((uint64_t)0x7008, (uint64_t)(uintptr_t)xarray_get(&xa, high + 1));
    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_set_range_overflow_guard) {
    struct xarray xa;
    xarray_init(&xa, 8);

    TEST_EXPECT_FALSE(xarray_set_range(&xa, UINT64_MAX, 2, (void *)(uintptr_t)0x1000));
    TEST_EXPECT_FALSE(xarray_set_range(&xa, 0, 2, (void *)(uintptr_t)UINT64_MAX));
    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_set_range_partial_over_compressed) {
    struct xarray xa;
    xarray_init(&xa, 8);

    TEST_ASSERT_TRUE(xarray_set_range(&xa, 0, 64, (void *)(uintptr_t)0x1000));
    TEST_ASSERT_TRUE(xarray_set_range(&xa, 10, 5, (void *)(uintptr_t)0x2000));

    TEST_EXPECT_EQ((uint64_t)0x1000, (uint64_t)(uintptr_t)xarray_get(&xa, 0));
    TEST_EXPECT_EQ((uint64_t)0x1048, (uint64_t)(uintptr_t)xarray_get(&xa, 9));
    TEST_EXPECT_EQ((uint64_t)0x2000, (uint64_t)(uintptr_t)xarray_get(&xa, 10));
    TEST_EXPECT_EQ((uint64_t)0x2020, (uint64_t)(uintptr_t)xarray_get(&xa, 14));
    TEST_EXPECT_EQ((uint64_t)0x1078, (uint64_t)(uintptr_t)xarray_get(&xa, 15));
    TEST_EXPECT_EQ((uint64_t)0x11f8, (uint64_t)(uintptr_t)xarray_get(&xa, 63));
    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_set_range_accepts_u64_max_single_index) {
    struct xarray xa;
    xarray_init(&xa, 8);

    TEST_ASSERT_TRUE(xarray_set_range(&xa, UINT64_MAX, 1, (void *)(uintptr_t)0x9000));
    TEST_EXPECT_EQ((uint64_t)0x9000, (uint64_t)(uintptr_t)xarray_get(&xa, UINT64_MAX));
    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_sparse_disjoint_ranges_keep_holes) {
    struct xarray xa;
    xarray_init(&xa, 8);

    TEST_ASSERT_TRUE(xarray_set_range(&xa, 3, 2, (void *)(uintptr_t)0x0000000012345000ull));
    TEST_ASSERT_TRUE(xarray_set_range(&xa, 4096, 3, (void *)(uintptr_t)0x00007fff0000a000ull));
    TEST_ASSERT_TRUE(
        xarray_set_range(&xa, (1ull << 20) + 7, 2, (void *)(uintptr_t)0xffff900000123000ull));

    TEST_EXPECT_EQ((uint64_t)0x0000000012345000ull, (uint64_t)(uintptr_t)xarray_get(&xa, 3));
    TEST_EXPECT_EQ((uint64_t)0x0000000012345008ull, (uint64_t)(uintptr_t)xarray_get(&xa, 4));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, 5));

    TEST_EXPECT_EQ((uint64_t)0x00007fff0000a000ull, (uint64_t)(uintptr_t)xarray_get(&xa, 4096));
    TEST_EXPECT_EQ((uint64_t)0x00007fff0000a010ull, (uint64_t)(uintptr_t)xarray_get(&xa, 4098));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, 4099));

    TEST_EXPECT_EQ(
        (uint64_t)0xffff900000123000ull, (uint64_t)(uintptr_t)xarray_get(&xa, (1ull << 20) + 7));
    TEST_EXPECT_EQ(
        (uint64_t)0xffff900000123008ull, (uint64_t)(uintptr_t)xarray_get(&xa, (1ull << 20) + 8));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, (1ull << 20) + 9));
    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_sparse_set_and_range_interleave) {
    struct xarray xa;
    xarray_init(&xa, 8);

    TEST_ASSERT_TRUE(xarray_set(&xa, 1, (void *)(uintptr_t)0x00000000abc01000ull));
    TEST_ASSERT_TRUE(xarray_set(&xa, 5000, (void *)(uintptr_t)0x0000008000005000ull));
    TEST_ASSERT_TRUE(xarray_set_range(&xa, 4998, 2, (void *)(uintptr_t)0xffffb00000007000ull));
    TEST_ASSERT_TRUE(
        xarray_set_range(&xa, 1ull << 24, 1, (void *)(uintptr_t)0x00007ffffff09000ull));

    TEST_EXPECT_EQ((uint64_t)0x00000000abc01000ull, (uint64_t)(uintptr_t)xarray_get(&xa, 1));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, 2));

    TEST_EXPECT_EQ((uint64_t)0xffffb00000007000ull, (uint64_t)(uintptr_t)xarray_get(&xa, 4998));
    TEST_EXPECT_EQ((uint64_t)0xffffb00000007008ull, (uint64_t)(uintptr_t)xarray_get(&xa, 4999));
    TEST_EXPECT_EQ((uint64_t)0x0000008000005000ull, (uint64_t)(uintptr_t)xarray_get(&xa, 5000));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, 5001));

    TEST_EXPECT_EQ(
        (uint64_t)0x00007ffffff09000ull, (uint64_t)(uintptr_t)xarray_get(&xa, 1ull << 24));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, (1ull << 24) + 1));
    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_set_range_very_large_span) {
    struct xarray xa;
    xarray_init(&xa, 8);

    uint64_t start_index = 1ull << 34;
    uint64_t len = 1ull << 22;
    uint64_t start_value = 0x0000555500000000ull;
    uint64_t last_index = start_index + len - 1;

    TEST_ASSERT_TRUE(xarray_set_range(&xa, start_index, len, (void *)(uintptr_t)start_value));

    TEST_EXPECT_EQ(start_value, (uint64_t)(uintptr_t)xarray_get(&xa, start_index));
    TEST_EXPECT_EQ(start_value + ((len / 2) * 8),
        (uint64_t)(uintptr_t)xarray_get(&xa, start_index + (len / 2)));
    TEST_EXPECT_EQ(start_value + ((len - 1) * 8), (uint64_t)(uintptr_t)xarray_get(&xa, last_index));

    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, start_index - 1));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, last_index + 1));
    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_set_range_unaligned_across_upper_slots) {
    struct xarray xa;
    xarray_init(&xa, 8);

    uint64_t start_index = (1ull << 18) + 5;
    uint64_t len = 5000;
    uint64_t start_value = 0x0000123400000000ull;
    uint64_t last_index = start_index + len - 1;

    TEST_ASSERT_TRUE(xarray_set(&xa, start_index - 2, (void *)(uintptr_t)0x0000000000001110ull));
    TEST_ASSERT_TRUE(xarray_set(&xa, last_index + 2, (void *)(uintptr_t)0x0000000000002220ull));

    TEST_ASSERT_TRUE(xarray_set_range(&xa, start_index, len, (void *)(uintptr_t)start_value));

    TEST_EXPECT_EQ(
        (uint64_t)0x0000000000001110ull, (uint64_t)(uintptr_t)xarray_get(&xa, start_index - 2));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, start_index - 1));
    TEST_EXPECT_EQ(start_value, (uint64_t)(uintptr_t)xarray_get(&xa, start_index));
    TEST_EXPECT_EQ(start_value + (59 * 8), (uint64_t)(uintptr_t)xarray_get(&xa, start_index + 59));
    TEST_EXPECT_EQ(start_value + (60 * 8), (uint64_t)(uintptr_t)xarray_get(&xa, start_index + 60));
    TEST_EXPECT_EQ(
        start_value + (4091 * 8), (uint64_t)(uintptr_t)xarray_get(&xa, start_index + 4091));
    TEST_EXPECT_EQ(
        start_value + (4092 * 8), (uint64_t)(uintptr_t)xarray_get(&xa, start_index + 4092));
    TEST_EXPECT_EQ(start_value + ((len - 1) * 8), (uint64_t)(uintptr_t)xarray_get(&xa, last_index));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, last_index + 1));
    TEST_EXPECT_EQ(
        (uint64_t)0x0000000000002220ull, (uint64_t)(uintptr_t)xarray_get(&xa, last_index + 2));

    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_destroy_empty_is_noop) {
    struct xarray xa;
    xarray_init(&xa, 8);

    xarray_destroy(&xa);

    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, 0));
    TEST_ASSERT_TRUE(xarray_set(&xa, 3, (void *)(uintptr_t)0x1000));
    TEST_EXPECT_EQ((uint64_t)0x1000, (uint64_t)(uintptr_t)xarray_get(&xa, 3));
    xarray_destroy(&xa);
}

DEFINE_UNIT_TEST(xarray_destroy_clears_and_reusable_after_sparse) {
    struct xarray xa;
    xarray_init(&xa, 8);

    TEST_ASSERT_TRUE(xarray_set_range(&xa, 1ull << 18, 128, (void *)(uintptr_t)0x2000));
    TEST_ASSERT_TRUE(xarray_set(&xa, UINT64_MAX, (void *)(uintptr_t)0x3000));
    TEST_EXPECT_EQ((uint64_t)0x2000, (uint64_t)(uintptr_t)xarray_get(&xa, 1ull << 18));
    TEST_EXPECT_EQ((uint64_t)0x3000, (uint64_t)(uintptr_t)xarray_get(&xa, UINT64_MAX));

    xarray_destroy(&xa);

    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, 1ull << 18));
    TEST_EXPECT_EQ((uint64_t)0, (uint64_t)(uintptr_t)xarray_get(&xa, UINT64_MAX));

    TEST_ASSERT_TRUE(xarray_set_range(&xa, 10, 2, (void *)(uintptr_t)0x5000));
    TEST_EXPECT_EQ((uint64_t)0x5000, (uint64_t)(uintptr_t)xarray_get(&xa, 10));
    TEST_EXPECT_EQ((uint64_t)0x5008, (uint64_t)(uintptr_t)xarray_get(&xa, 11));
    xarray_destroy(&xa);
}
