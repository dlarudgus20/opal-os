#include <opal/test.h>
#include <opal/utils/ringbuffer.h>

DEFINE_TEST(utils_ringbuffer_single_capacity) {
    unsigned storage;
    struct ringbuffer queue;
    ringbuffer_init(&queue, &storage, 1);
    TEST_EXPECT_TRUE(ringbuffer_is_empty(&queue));
    TEST_EXPECT_FALSE(ringbuffer_is_full(&queue));

    ringbuffer_push(&queue, unsigned, 100u);
    TEST_EXPECT_TRUE(ringbuffer_is_full(&queue));
    TEST_ASSERT_FALSE(ringbuffer_is_empty(&queue));
    TEST_EXPECT_EQ(100u, ringbuffer_pop(&queue, unsigned));
    TEST_EXPECT_TRUE(ringbuffer_is_empty(&queue));
    TEST_EXPECT_FALSE(ringbuffer_is_full(&queue));
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
