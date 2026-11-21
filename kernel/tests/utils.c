#include <opal/test.h>
#include <opal/utils/ringbuffer.h>
#include <kc/stdio.h>

DEFINE_TEST(utils_memmove_overlap) {
    char buffer[] = "abcdef";
    memmove(buffer + 1, buffer, 5);
    TEST_EXPECT_STREQ("aabcde", buffer);
    memmove(buffer, buffer + 1, 6);
    TEST_EXPECT_STREQ("abcde", buffer);
}

DEFINE_TEST(utils_snprintf) {
    char buffer[16];
    TEST_ASSERT_EQ(6, ksnprintf(buffer, sizeof(buffer), "%s-%u", "opal", 7u));
    TEST_EXPECT_STREQ("opal-7", buffer);
    TEST_EXPECT_EQ(6, ksnprintf(buffer, 5, "%s-%u", "opal", 7u));
    TEST_EXPECT_STREQ("opal", buffer);
}

DEFINE_TEST(utils_ringbuffer_wraparound) {
    unsigned storage[2];
    struct ringbuffer queue;
    ringbuffer_init(&queue, storage, sizeof(storage) / sizeof(storage[0]));

    ringbuffer_push(&queue, unsigned, 10u);
    ringbuffer_push(&queue, unsigned, 20u);
    TEST_ASSERT_TRUE(ringbuffer_is_full(&queue));
    TEST_EXPECT_EQ(10u, ringbuffer_pop(&queue, unsigned));

    ringbuffer_push(&queue, unsigned, 30u);
    TEST_EXPECT_EQ(20u, ringbuffer_pop(&queue, unsigned));
    TEST_EXPECT_EQ(30u, ringbuffer_pop(&queue, unsigned));
    TEST_EXPECT_TRUE(ringbuffer_is_empty(&queue));
}
