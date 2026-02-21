#include <opal/mm/buddy.h>
#include <opal/mm/map.h>
#include <opal/mm/pfn.h>
#include <opal/platform/mm/defines.h>
#include <opal/test.h>

DEFINE_UNIT_TEST(buddy_alloc_free_order0) {
    size_t before = mm_buddy_get_free_pages();
    pfn_t pfn = mm_buddy_alloc(0);
    TEST_ASSERT_FALSE(pfn == PFN_INVALID);
    TEST_EXPECT_EQ(before - 1, mm_buddy_get_free_pages());
    mm_buddy_free(pfn, 0);
    TEST_EXPECT_EQ(before, mm_buddy_get_free_pages());
}

DEFINE_UNIT_TEST(buddy_alloc_free_order1) {
    if (mm_buddy_get_max_order() < 1) {
        return;
    }

    size_t before = mm_buddy_get_free_pages();
    pfn_t pfn = mm_buddy_alloc(1);
    TEST_ASSERT_FALSE(pfn == PFN_INVALID);
    TEST_EXPECT_EQ(before - 2, mm_buddy_get_free_pages());
    mm_buddy_free(pfn, 1);
    TEST_EXPECT_EQ(before, mm_buddy_get_free_pages());
}

DEFINE_UNIT_TEST(buddy_alloc_invalid_order) {
    uint8_t max_order = mm_buddy_get_max_order();
    pfn_t pfn = mm_buddy_alloc((uint8_t)(max_order + 1));
    TEST_EXPECT_EQ(PFN_INVALID, pfn);
}

DEFINE_UNIT_TEST(buddy_alloc_returns_valid_aligned_pfn) {
    uint8_t order = mm_buddy_get_max_order() >= 2 ? 2 : 0;
    pfn_t pfn = mm_buddy_alloc(order);
    TEST_ASSERT_FALSE(pfn == PFN_INVALID);
    TEST_EXPECT_TRUE(mm_pfn_is_valid(pfn));
    TEST_EXPECT_EQ(0, pfn & (((pfn_t)1 << order) - 1));
    mm_buddy_free(pfn, order);
}

DEFINE_UNIT_TEST(buddy_multi_alloc_free_restores_count) {
    size_t before = mm_buddy_get_free_pages();
    pfn_t a = mm_buddy_alloc(0);
    pfn_t b = mm_buddy_alloc(0);

    TEST_ASSERT_FALSE(a == PFN_INVALID);
    TEST_ASSERT_FALSE(b == PFN_INVALID);
    TEST_EXPECT_FALSE(a == b);
    TEST_EXPECT_EQ(before - 2, mm_buddy_get_free_pages());

    mm_buddy_free(a, 0);
    mm_buddy_free(b, 0);
    TEST_EXPECT_EQ(before, mm_buddy_get_free_pages());
}
