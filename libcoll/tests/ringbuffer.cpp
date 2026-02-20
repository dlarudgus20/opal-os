#include "test-pch.h"

extern "C" {
#include <collections/ringbuffer.h>
}

struct testbuffer {
    void* buffer;
    ringbuffer rb;
    explicit testbuffer(size_t buffer_size, size_t maxlen) {
        buffer = malloc(buffer_size);
        if (!buffer) {
            throw std::bad_alloc();
        }
        ringbuffer_init(&rb, buffer, maxlen);
    }
    ~testbuffer() {
        free(buffer);
    }
    ringbuffer* get() {
        return &rb;
    }
    ringbuffer* operator ->() {
        return &rb;
    }
};

TEST(ringbuffer_test, initializes_correctly) {
    testbuffer rb(5 * sizeof(int), 5);
    EXPECT_EQ(rb->buffer, rb.buffer);
    EXPECT_EQ(rb->head, 0);
    EXPECT_EQ(rb->tail, 0);
    EXPECT_EQ(rb->count, 0);
    EXPECT_EQ(rb->maxlen, 5);
    EXPECT_TRUE(ringbuffer_is_empty(rb.get()));
    EXPECT_FALSE(ringbuffer_is_full(rb.get()));
}

TEST(ringbuffer_test, push_and_pop_single_element) {
    testbuffer rb(5 * sizeof(int), 5);

    ringbuffer_push(rb.get(), int, 42);
    EXPECT_EQ(rb->count, 1);
    EXPECT_FALSE(ringbuffer_is_empty(rb.get()));

    int val = ringbuffer_pop(rb.get(), int);
    EXPECT_EQ(val, 42);
    EXPECT_EQ(rb->count, 0);
    EXPECT_TRUE(ringbuffer_is_empty(rb.get()));
}

TEST(ringbuffer_test, fills_to_capacity) {
    testbuffer rb(5 * sizeof(int), 5);

    for (int i = 0; i < 5; i++) {
        ringbuffer_push(rb.get(), int, i);
    }

    EXPECT_TRUE(ringbuffer_is_full(rb.get()));
    EXPECT_EQ(rb->count, 5);
}

TEST(ringbuffer_test, wraps_around_correctly) {
    testbuffer rb(5 * sizeof(int), 5);

    // Fill buffer
    for (int i = 0; i < 5; i++) {
        ringbuffer_push(rb.get(), int, i);
    }

    // Pop two elements
    EXPECT_EQ(ringbuffer_pop(rb.get(), int), 0);
    EXPECT_EQ(ringbuffer_pop(rb.get(), int), 1);

    // Push two more (should wrap around)
    ringbuffer_push(rb.get(), int, 10);
    ringbuffer_push(rb.get(), int, 11);

    // Check remaining elements
    EXPECT_EQ(ringbuffer_pop(rb.get(), int), 2);
    EXPECT_EQ(ringbuffer_pop(rb.get(), int), 3);
    EXPECT_EQ(ringbuffer_pop(rb.get(), int), 4);
    EXPECT_EQ(ringbuffer_pop(rb.get(), int), 10);
    EXPECT_EQ(ringbuffer_pop(rb.get(), int), 11);
}

TEST(ringbuffer_test, index_wrapping) {
    testbuffer rb(5 * sizeof(int), 5);

    // Test tail wrapping
    for (int i = 0; i < 5; i++) {
        size_t idx = ringbuffer_push_index(rb.get());
        EXPECT_EQ(idx, i);
    }
    EXPECT_EQ(rb->tail, 0); // Should wrap to 0

    // Test head wrapping
    for (int i = 0; i < 5; i++) {
        size_t idx = ringbuffer_pop_index(rb.get());
        EXPECT_EQ(idx, i);
    }
    EXPECT_EQ(rb->head, 0); // Should wrap to 0
}

TEST(test_ringbuffer, single_capacity) {
    testbuffer rb(sizeof(int), 1);

    EXPECT_TRUE(ringbuffer_is_empty(rb.get()));
    EXPECT_FALSE(ringbuffer_is_full(rb.get()));

    ringbuffer_push(rb.get(), int, 100);
    EXPECT_FALSE(ringbuffer_is_empty(rb.get()));
    EXPECT_TRUE(ringbuffer_is_full(rb.get()));

    int val = ringbuffer_pop(rb.get(), int);
    EXPECT_EQ(val, 100);
    EXPECT_TRUE(ringbuffer_is_empty(rb.get()));
    EXPECT_FALSE(ringbuffer_is_full(rb.get()));
}

TEST(test_ringbuffer, alternating_push_pop) {
    testbuffer rb(3 * sizeof(int), 3);

    for (int i = 0; i < 10; i++) {
        ringbuffer_push(rb.get(), int, i);
        EXPECT_EQ(rb->count, 1);

        int val = ringbuffer_pop(rb.get(), int);
        EXPECT_EQ(val, i);
        EXPECT_EQ(rb->count, 0);
    }
}

TEST(test_ringbuffer, partial_fill_and_drain) {
    testbuffer rb(5 * sizeof(int), 5);

    // Fill partially
    for (int i = 0; i < 3; i++) {
        ringbuffer_push(rb.get(), int, i);
    }
    EXPECT_EQ(rb->count, 3);
    EXPECT_FALSE(ringbuffer_is_full(rb.get()));

    // Drain partially
    for (int i = 0; i < 2; i++) {
        int val = ringbuffer_pop(rb.get(), int);
        EXPECT_EQ(val, i);
    }
    EXPECT_EQ(rb->count, 1);

    // Check remaining
    int val = ringbuffer_pop(rb.get(), int);
    EXPECT_EQ(val, 2);
    EXPECT_TRUE(ringbuffer_is_empty(rb.get()));
}

TEST(test_ringbuffer, multiple_wrap_cycles) {
    testbuffer rb(3 * sizeof(int), 3);

    // Multiple complete cycles
    for (int cycle = 0; cycle < 3; cycle++) {
        // Fill completely
        for (int i = 0; i < 3; i++) {
            ringbuffer_push(rb.get(), int, cycle * 10 + i);
        }
        EXPECT_TRUE(ringbuffer_is_full(rb.get()));

        // Drain completely
        for (int i = 0; i < 3; i++) {
            int val = ringbuffer_pop(rb.get(), int);
            EXPECT_EQ(val, cycle * 10 + i);
        }
        EXPECT_TRUE(ringbuffer_is_empty(rb.get()));
    }
}

TEST(test_ringbuffer, push_to_full_buffer_death) {
    testbuffer rb(3 * sizeof(int), 3);

    for (int i = 0; i < 3; i++) {
        ringbuffer_push(rb.get(), int, i);
    }

    EXPECT_DEATH(ringbuffer_push(rb.get(), int, 10), "ringbuffer is full");
}

TEST(test_ringbuffer, pop_from_empty_buffer_death) {
    testbuffer rb(3 * sizeof(int), 3);
    EXPECT_DEATH((void)ringbuffer_pop(rb.get(), int), "ringbuffer is empty");
}

TEST(test_ringbuffer, different_data_types) {
    // Test with different sized types
    testbuffer rb_char(4, 4);
    testbuffer rb_double(4 * sizeof(double), 4);

    // Char test
    for (char c = 'A'; c <= 'D'; c++) {
        ringbuffer_push(rb_char.get(), char, c);
    }
    for (char c = 'A'; c <= 'D'; c++) {
        char val = ringbuffer_pop(rb_char.get(), char);
        EXPECT_EQ(val, c);
    }

    // Double test
    for (int i = 0; i < 4; i++) {
        ringbuffer_push(rb_double.get(), double, i * 3.14);
    }
    for (int i = 0; i < 4; i++) {
        double val = ringbuffer_pop(rb_double.get(), double);
        EXPECT_DOUBLE_EQ(val, i * 3.14);
    }
}

TEST(test_ringbuffer, stress_random_operations) {
    testbuffer rb(10 * sizeof(int), 10);
    int expected_values[10];
    int front = 0, count = 0;

    // Simulate random push/pop operations
    for (int op = 0; op < 100; op++) {
        bool should_push = (count == 0) || ((count < 10) && (op % 3 != 0));

        if (should_push) {
            expected_values[(front + count) % 10] = op;
            ringbuffer_push(rb.get(), int, op);
            count++;
        } else {
            int expected = expected_values[front];
            int actual = ringbuffer_pop(rb.get(), int);
            EXPECT_EQ(actual, expected);
            front = (front + 1) % 10;
            count--;
        }

        EXPECT_EQ(rb->count, count);
        EXPECT_EQ(ringbuffer_is_empty(rb.get()), count == 0);
        EXPECT_EQ(ringbuffer_is_full(rb.get()), count == 10);
    }
}

TEST(test_ringbuffer, index_boundary_conditions) {
    testbuffer rb(2 * sizeof(int), 2);

    // Test maximum index values
    size_t idx1 = ringbuffer_push_index(rb.get());
    EXPECT_EQ(idx1, 0);
    EXPECT_EQ(rb->tail, 1);

    size_t idx2 = ringbuffer_push_index(rb.get());
    EXPECT_EQ(idx2, 1);
    EXPECT_EQ(rb->tail, 0); // Wrapped

    size_t pop_idx1 = ringbuffer_pop_index(rb.get());
    EXPECT_EQ(pop_idx1, 0);
    EXPECT_EQ(rb->head, 1);

    size_t pop_idx2 = ringbuffer_pop_index(rb.get());
    EXPECT_EQ(pop_idx2, 1);
    EXPECT_EQ(rb->head, 0); // Wrapped
}

TEST(test_ringbuffer, large_buffer) {
    const size_t large_size = 1000;
    testbuffer rb(large_size * sizeof(int), large_size);

    // Fill completely
    for (size_t i = 0; i < large_size; i++) {
        ringbuffer_push(rb.get(), int, i);
    }
    EXPECT_TRUE(ringbuffer_is_full(rb.get()));

    // Drain completely
    for (size_t i = 0; i < large_size; i++) {
        int val = ringbuffer_pop(rb.get(), int);
        EXPECT_EQ(val, i);
    }
    EXPECT_TRUE(ringbuffer_is_empty(rb.get()));
}
