#include <stddef.h>
#include <stdint.h>

#include <kc/string.h>

#include <opal/mm/slab.h>
#include <opal/test.h>

static void expect_all_zero(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        TEST_ASSERT_EQ(0, buf[i]);
    }
}

DEFINE_UNIT_TEST(slab_cache_init_and_basic_alloc_free) {
    struct slab cache = {0};

    slab_create(&cache, 32, 8);
    TEST_EXPECT_EQ(32, slab_get_object_size(&cache));
    TEST_EXPECT_EQ(0, slab_get_total(&cache));
    TEST_EXPECT_EQ(0, slab_get_inuse(&cache));

    void *ptr = slab_alloc(&cache);
    TEST_ASSERT_TRUE(ptr != NULL);
    TEST_EXPECT_EQ(1, slab_get_inuse(&cache));
    TEST_EXPECT_TRUE(slab_get_total(&cache) >= 1);

    slab_free(&cache, ptr);
    TEST_EXPECT_EQ(0, slab_get_inuse(&cache));

    slab_destroy(&cache);
}

DEFINE_UNIT_TEST(slab_cache_keeps_alignment) {
    struct slab cache = {0};
    slab_create(&cache, 24, 16);

    void *ptr = slab_alloc(&cache);
    TEST_ASSERT_TRUE(ptr != NULL);
    TEST_EXPECT_EQ(0, ((uintptr_t)ptr) & 0x0f);

    slab_free(&cache, ptr);
    slab_destroy(&cache);
}

DEFINE_UNIT_TEST(slab_cache_multiple_alloc_free_restore_counts) {
    struct slab cache = {0};
    void *ptrs[128];
    size_t count = 0;

    slab_create(&cache, 64, 8);

    for (size_t i = 0; i < 128; i++) {
        ptrs[i] = slab_alloc(&cache);
        if (!ptrs[i]) {
            break;
        }
        count++;
    }

    TEST_EXPECT_EQ(count, slab_get_inuse(&cache));
    TEST_EXPECT_TRUE(slab_get_total(&cache) >= count);

    for (size_t i = 0; i < count; i++) {
        slab_free(&cache, ptrs[i]);
    }

    TEST_EXPECT_EQ(0, slab_get_inuse(&cache));
    slab_destroy(&cache);
}

DEFINE_UNIT_TEST(slab_cache_alloc_returns_zeroed_payload) {
    struct slab cache = {0};
    slab_create(&cache, 48, 8);

    uint8_t *ptr = slab_alloc(&cache);
    TEST_ASSERT_TRUE(ptr != NULL);
    expect_all_zero(ptr, 48);

    slab_free(&cache, ptr);
    slab_destroy(&cache);
}

DEFINE_UNIT_TEST(slab_cache_realloc_rezeroes_payload) {
    struct slab cache = {0};
    slab_create(&cache, 40, 8);

    uint8_t *ptr = slab_alloc(&cache);
    TEST_ASSERT_TRUE(ptr != NULL);
    memset(ptr, 0xab, 40);
    slab_free(&cache, ptr);

    uint8_t *ptr2 = slab_alloc(&cache);
    TEST_ASSERT_TRUE(ptr2 != NULL);
    expect_all_zero(ptr2, 40);

    slab_free(&cache, ptr2);
    slab_destroy(&cache);
}

DEFINE_UNIT_TEST(slab_cache_two_instances_are_independent) {
    struct slab a = {0};
    struct slab b = {0};
    slab_create(&a, 32, 8);
    slab_create(&b, 64, 16);

    void *ap = slab_alloc(&a);
    void *bp = slab_alloc(&b);
    TEST_ASSERT_TRUE(ap != NULL);
    TEST_ASSERT_TRUE(bp != NULL);
    TEST_EXPECT_FALSE(ap == bp);

    TEST_EXPECT_EQ(1, slab_get_inuse(&a));
    TEST_EXPECT_EQ(1, slab_get_inuse(&b));
    TEST_EXPECT_TRUE(slab_get_total(&a) >= 1);
    TEST_EXPECT_TRUE(slab_get_total(&b) >= 1);

    slab_free(&a, ap);
    slab_free(&b, bp);
    TEST_EXPECT_EQ(0, slab_get_inuse(&a));
    TEST_EXPECT_EQ(0, slab_get_inuse(&b));

    slab_destroy(&a);
    slab_destroy(&b);
}

DEFINE_UNIT_TEST(slab_cache_total_grows_after_first_page_is_full) {
    struct slab cache = {0};
    void *ptrs[512];
    size_t count = 0;

    slab_create(&cache, 128, 16);

    void *first = slab_alloc(&cache);
    TEST_ASSERT_TRUE(first != NULL);
    ptrs[count++] = first;

    size_t first_page_capacity = slab_get_total(&cache);
    TEST_ASSERT_TRUE(first_page_capacity > 0);

    bool expanded = false;
    for (size_t i = 0; i < first_page_capacity + 8 && count < 512; i++) {
        void *ptr = slab_alloc(&cache);
        TEST_ASSERT_TRUE(ptr != NULL);
        ptrs[count++] = ptr;
        if (slab_get_total(&cache) > first_page_capacity) {
            expanded = true;
            break;
        }
    }

    TEST_EXPECT_TRUE(expanded);

    for (size_t i = 0; i < count; i++) {
        slab_free(&cache, ptrs[i]);
    }
    TEST_EXPECT_EQ(0, slab_get_inuse(&cache));
    slab_destroy(&cache);
}

DEFINE_UNIT_TEST(slab_cache_alloc_dealloc_pattern_with_gaps) {
    struct slab cache = {0};
    void *ptrs[30];
    void *new_ptrs[10];

    slab_create(&cache, 24, 8);

    for (size_t i = 0; i < 30; i++) {
        ptrs[i] = slab_alloc(&cache);
        TEST_ASSERT_TRUE(ptrs[i] != NULL);
    }
    TEST_EXPECT_EQ(30, slab_get_inuse(&cache));

    for (size_t i = 0; i < 30; i += 3) {
        slab_free(&cache, ptrs[i]);
        ptrs[i] = NULL;
    }
    TEST_EXPECT_EQ(20, slab_get_inuse(&cache));

    for (size_t i = 0; i < 10; i++) {
        new_ptrs[i] = slab_alloc(&cache);
        TEST_ASSERT_TRUE(new_ptrs[i] != NULL);
    }
    TEST_EXPECT_EQ(30, slab_get_inuse(&cache));

    for (size_t i = 0; i < 30; i++) {
        if ((i % 3) != 0) {
            slab_free(&cache, ptrs[i]);
        } else {
            TEST_ASSERT_TRUE(ptrs[i] == NULL);
        }
    }
    for (size_t i = 0; i < 10; i++) {
        slab_free(&cache, new_ptrs[i]);
    }

    TEST_EXPECT_EQ(0, slab_get_inuse(&cache));
    slab_destroy(&cache);
}

DEFINE_UNIT_TEST(slab_cache_fragmentation_and_reuse) {
    struct slab cache = {0};
    void *ptrs[100];
    void *reused[50];

    slab_create(&cache, 40, 8);

    for (size_t i = 0; i < 100; i++) {
        ptrs[i] = slab_alloc(&cache);
        TEST_ASSERT_TRUE(ptrs[i] != NULL);
    }
    TEST_EXPECT_EQ(100, slab_get_inuse(&cache));

    for (size_t i = 0; i < 100; i += 2) {
        slab_free(&cache, ptrs[i]);
    }
    TEST_EXPECT_EQ(50, slab_get_inuse(&cache));

    const size_t total_before_reuse = slab_get_total(&cache);
    for (size_t i = 0; i < 50; i++) {
        reused[i] = slab_alloc(&cache);
        TEST_ASSERT_TRUE(reused[i] != NULL);
    }
    TEST_EXPECT_EQ(total_before_reuse, slab_get_total(&cache));
    TEST_EXPECT_EQ(100, slab_get_inuse(&cache));

    for (size_t i = 1; i < 100; i += 2) {
        slab_free(&cache, ptrs[i]);
    }
    for (size_t i = 0; i < 50; i++) {
        slab_free(&cache, reused[i]);
    }

    TEST_EXPECT_EQ(0, slab_get_inuse(&cache));
    slab_destroy(&cache);
}

DEFINE_UNIT_TEST(slab_cache_interleaved_allocation_and_deallocation) {
    struct slab cache = {0};
    void *ptrs[100];
    size_t count = 0;

    slab_create(&cache, 64, 8);

    for (size_t i = 0; i < 100; i++) {
        if ((i % 3) == 0 && count > 0) {
            slab_free(&cache, ptrs[count - 1]);
            count--;
        } else {
            void *ptr = slab_alloc(&cache);
            TEST_ASSERT_TRUE(ptr != NULL);
            ptrs[count++] = ptr;
        }
    }

    for (size_t i = 0; i < count; i++) {
        slab_free(&cache, ptrs[i]);
    }
    TEST_EXPECT_EQ(0, slab_get_inuse(&cache));

    void *ptr = slab_alloc(&cache);
    TEST_ASSERT_TRUE(ptr != NULL);
    slab_free(&cache, ptr);
    slab_destroy(&cache);
}

DEFINE_UNIT_TEST(slab_cache_stress_allocation_and_deallocation) {
    struct slab cache = {0};
    void *ptrs[1000];
    size_t count = 0;

    slab_create(&cache, 256, 32);

    for (size_t i = 0; i < 1000; i++) {
        if ((i % 5) == 0 && count > 0) {
            slab_free(&cache, ptrs[count - 1]);
            count--;
        } else {
            void *ptr = slab_alloc(&cache);
            TEST_ASSERT_TRUE(ptr != NULL);
            ptrs[count++] = ptr;
        }
    }

    for (size_t i = 0; i < count; i++) {
        slab_free(&cache, ptrs[i]);
    }
    TEST_EXPECT_EQ(0, slab_get_inuse(&cache));

    void *ptr = slab_alloc(&cache);
    TEST_ASSERT_TRUE(ptr != NULL);
    slab_free(&cache, ptr);
    slab_destroy(&cache);
}
