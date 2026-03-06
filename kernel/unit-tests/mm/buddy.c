#include <opal/test.h>
#include <opal/mm/mm.h>
#include <opal/mm/buddy.h>
#include <opal/mm/map.h>
#include <opal/mm/pfn.h>
#include <opal/platform/asm.h>
#include <opal/platform/mm/defines.h>

DEFINE_UNIT_TEST(buddy_alloc_free_order0) {
    interrupts_disable();
    struct buddy *buddy = mm_get_buddy();
    size_t before = buddy_get_free_pages(buddy);
    pfn_t pfn = buddy_alloc(buddy, 0);
    TEST_ASSERT_FALSE(pfn == PFN_INVALID);
    TEST_EXPECT_EQ(before - 1, buddy_get_free_pages(buddy));
    buddy_free(buddy, pfn, 0);
    TEST_EXPECT_EQ(before, buddy_get_free_pages(buddy));
}

DEFINE_UNIT_TEST(buddy_alloc_free_order1) {
    interrupts_disable();

    struct buddy *buddy = mm_get_buddy();
    if (buddy_get_max_order(buddy) < 1) {
        return;
    }

    size_t before = buddy_get_free_pages(buddy);
    pfn_t pfn = buddy_alloc(buddy, 1);
    TEST_ASSERT_FALSE(pfn == PFN_INVALID);
    TEST_EXPECT_EQ(before - 2, buddy_get_free_pages(buddy));
    buddy_free(buddy, pfn, 1);
    TEST_EXPECT_EQ(before, buddy_get_free_pages(buddy));
}

DEFINE_UNIT_TEST(buddy_alloc_invalid_order) {
    interrupts_disable();
    struct buddy *buddy = mm_get_buddy();
    uint8_t max_order = buddy_get_max_order(buddy);
    pfn_t pfn = buddy_alloc(buddy, (uint8_t)(max_order + 1));
    TEST_EXPECT_EQ(PFN_INVALID, pfn);
}

DEFINE_UNIT_TEST(buddy_alloc_returns_valid_aligned_pfn) {
    interrupts_disable();
    struct buddy *buddy = mm_get_buddy();
    uint8_t order = buddy_get_max_order(buddy) >= 2 ? 2 : 0;
    pfn_t pfn = buddy_alloc(buddy, order);
    TEST_ASSERT_FALSE(pfn == PFN_INVALID);
    TEST_EXPECT_TRUE(mm_pfn_is_valid(pfn));
    TEST_EXPECT_EQ(0, pfn & (((pfn_t)1 << order) - 1));
    buddy_free(buddy, pfn, order);
}

DEFINE_UNIT_TEST(buddy_multi_alloc_free_restores_count) {
    interrupts_disable();

    struct buddy *buddy = mm_get_buddy();
    size_t before = buddy_get_free_pages(buddy);
    pfn_t a = buddy_alloc(buddy, 0);
    pfn_t b = buddy_alloc(buddy, 0);

    TEST_ASSERT_FALSE(a == PFN_INVALID);
    TEST_ASSERT_FALSE(b == PFN_INVALID);
    TEST_EXPECT_FALSE(a == b);
    TEST_EXPECT_EQ(before - 2, buddy_get_free_pages(buddy));

    buddy_free(buddy, a, 0);
    buddy_free(buddy, b, 0);
    TEST_EXPECT_EQ(before, buddy_get_free_pages(buddy));
}
