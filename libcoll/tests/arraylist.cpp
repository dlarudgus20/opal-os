#include <gtest/gtest.h>

extern "C" {
#include <collections/arraylist.h>
}

#include <vector>
#include <algorithm>
#include <limits>
#include <cstring>

// Mock allocator for testing
class MockAllocator {
public:
    static std::vector<std::pair<void*, size_t>> allocated_blocks;
    static size_t total_allocated;
    static size_t allocation_count;
    static size_t deallocation_count;

    static void reset() {
        allocated_blocks.clear();
        total_allocated = 0;
        allocation_count = 0;
        deallocation_count = 0;
    }

    static void* alloc(size_t len, size_t* allocated_len) {
        void* ptr = malloc(len);
        if (ptr) {
            allocated_blocks.push_back({ptr, len});
            total_allocated += len;
            allocation_count++;
            *allocated_len = len;
        }
        return ptr;
    }

    static void dealloc(void* ptr, size_t len) {
        auto it = std::find_if(allocated_blocks.begin(), allocated_blocks.end(),
                              [ptr](const auto& p) { return p.first == ptr; });
        if (it != allocated_blocks.end()) {
            total_allocated -= it->second;
            allocated_blocks.erase(it);
            deallocation_count++;
        }
        free(ptr);
    }

    static size_t shrink(void* ptr, size_t old_len, size_t new_len) {
        return old_len;
    }
};

std::vector<std::pair<void*, size_t>> MockAllocator::allocated_blocks;
size_t MockAllocator::total_allocated = 0;
size_t MockAllocator::allocation_count = 0;
size_t MockAllocator::deallocation_count = 0;

class OverflowRecorderAllocator {
public:
    static size_t last_requested_len;
    static size_t alloc_calls;
    static char dummy;

    static void reset() {
        last_requested_len = 0;
        alloc_calls = 0;
    }

    static void* alloc(size_t len, size_t* allocated_len) {
        last_requested_len = len;
        alloc_calls += 1;
        *allocated_len = len;
        return &dummy;
    }

    static void dealloc(void*, size_t) {
    }

    static size_t shrink(void*, size_t old_len, size_t) {
        return old_len;
    }
};

size_t OverflowRecorderAllocator::last_requested_len = 0;
size_t OverflowRecorderAllocator::alloc_calls = 0;
char OverflowRecorderAllocator::dummy = 0;

class arraylist_test : public ::testing::Test {
protected:
    void SetUp() override {
        MockAllocator::reset();
        allocator = {
            MockAllocator::alloc,
            MockAllocator::dealloc,
            MockAllocator::shrink
        };
    }

    void TearDown() override {
        // Check for memory leaks
        ASSERT_EQ(MockAllocator::total_allocated, 0) << "Memory leak detected!";
    }

    struct arraylist_allocator allocator;
};

TEST(arraylist_overflow_test, push_back_size_addition_must_not_wrap) {
    OverflowRecorderAllocator::reset();

    struct arraylist list;
    list.pa = {
        OverflowRecorderAllocator::alloc,
        OverflowRecorderAllocator::dealloc,
        OverflowRecorderAllocator::shrink
    };
    list.data = &OverflowRecorderAllocator::dummy;
    list.size = std::numeric_limits<size_t>::max() - 3;
    list.capacity = list.size;

    ASSERT_DEATH({
        struct arraylist local = list;
        (void)arraylist_push_back(&local, 8);
    }, "arraylist: push_back size overflow");
}

TEST(arraylist_overflow_test, insert_size_addition_must_not_wrap) {
    OverflowRecorderAllocator::reset();

    struct arraylist list;
    list.pa = {
        OverflowRecorderAllocator::alloc,
        OverflowRecorderAllocator::dealloc,
        OverflowRecorderAllocator::shrink
    };
    list.data = &OverflowRecorderAllocator::dummy;
    list.size = std::numeric_limits<size_t>::max() - 3;
    list.capacity = list.size;

    ASSERT_DEATH({
        struct arraylist local = list;
        (void)arraylist_insert(&local, local.size, 8);
    }, "arraylist: insert size overflow");
}

TEST(arraylist_macro_test, foreach_empty_list_with_null_data_runs_zero_times) {
    struct arraylist list = {};
    list.data = nullptr;
    list.size = 0;
    list.capacity = 0;

    int iterations = 0;
    arraylist_foreach(int*, p, &list) {
        (void)p;
        iterations += 1;
    }
    ASSERT_EQ(iterations, 0);
}

TEST_F(arraylist_test, init_empty_list) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    ASSERT_EQ(list.size, 0);
    ASSERT_EQ(list.capacity, 0);
    ASSERT_EQ(list.data, nullptr);
    ASSERT_EQ(MockAllocator::allocation_count, 0);
}

TEST_F(arraylist_test, init_with_capacity) {
    struct arraylist list;
    arraylist_init(&list, 10, &allocator);

    ASSERT_EQ(list.size, 0);
    ASSERT_EQ(list.capacity, 10);
    ASSERT_NE(list.data, nullptr);
    ASSERT_EQ(MockAllocator::allocation_count, 1);

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, push_back_single_element) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
    *ptr = 42;

    ASSERT_EQ(list.size, sizeof(int));
    ASSERT_GE(list.capacity, sizeof(int));
    ASSERT_EQ(arraylist_at(&list, int, 0), 42);

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, push_back_multiple_elements) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    for (int i = 0; i < 10; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;
    }

    ASSERT_EQ(list.size, 10 * sizeof(int));
    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), i);
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, pop_back) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    // Push some elements
    for (int i = 0; i < 5; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;
    }

    // Pop last element
    arraylist_pop_back(&list, sizeof(int));

    ASSERT_EQ(list.size, 4 * sizeof(int));
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), i);
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, reserve) {
    struct arraylist list;
    arraylist_init(&list, 5, &allocator);

    arraylist_reserve(&list, 20);

    ASSERT_EQ(list.size, 0);
    ASSERT_GE(list.capacity, 20);

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, resize_grow) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    arraylist_resize(&list, 10 * sizeof(int));

    ASSERT_EQ(list.size, 10 * sizeof(int));
    ASSERT_GE(list.capacity, 10 * sizeof(int));

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, resize_shrink) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    // First grow
    for (int i = 0; i < 10; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;
    }

    // Then shrink
    arraylist_resize(&list, 5 * sizeof(int));

    ASSERT_EQ(list.size, 5 * sizeof(int));
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), i);
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, insert_at_beginning) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    // Add some elements first
    for (int i = 1; i <= 3; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;
    }

    // Insert at beginning
    int* ptr = (int*)arraylist_insert(&list, 0, sizeof(int));
    *ptr = 0;

    ASSERT_EQ(list.size, 4 * sizeof(int));
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), i);
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, insert_at_middle) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    // Add elements: [0, 1, 3, 4]
    int values[] = {0, 1, 3, 4};
    for (int i = 0; i < 4; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = values[i];
    }

    // Insert 2 at position 2
    int* ptr = (int*)arraylist_insert(&list, 2 * sizeof(int), sizeof(int));
    *ptr = 2;

    ASSERT_EQ(list.size, 5 * sizeof(int));
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), i);
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, insert_at_end) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    // Add some elements
    for (int i = 0; i < 3; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;
    }

    // Insert at end (should be equivalent to push_back)
    int* ptr = (int*)arraylist_insert(&list, 3 * sizeof(int), sizeof(int));
    *ptr = 3;

    ASSERT_EQ(list.size, 4 * sizeof(int));
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), i);
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, remove_from_beginning) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    // Add elements [0, 1, 2, 3, 4]
    for (int i = 0; i < 5; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;
    }

    // Remove first element
    arraylist_remove(&list, 0, sizeof(int));

    ASSERT_EQ(list.size, 4 * sizeof(int));
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), i + 1);
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, remove_from_middle) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    // Add elements [0, 1, 2, 3, 4]
    for (int i = 0; i < 5; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;
    }

    // Remove middle element (index 2)
    arraylist_remove(&list, 2 * sizeof(int), sizeof(int));

    ASSERT_EQ(list.size, 4 * sizeof(int));
    int expected[] = {0, 1, 3, 4};
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), expected[i]);
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, remove_from_end) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    // Add elements [0, 1, 2, 3, 4]
    for (int i = 0; i < 5; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;
    }

    // Remove last element
    arraylist_remove(&list, 4 * sizeof(int), sizeof(int));

    ASSERT_EQ(list.size, 4 * sizeof(int));
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), i);
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, shrink_to_zero) {
    struct arraylist list;
    arraylist_init(&list, 10, &allocator);

    // Add some elements
    for (int i = 0; i < 5; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;
    }

    arraylist_shrink_to(&list, 0);

    ASSERT_EQ(list.size, 0);
    ASSERT_EQ(list.capacity, 0);
    ASSERT_EQ(list.data, nullptr);
}

TEST_F(arraylist_test, capacity_growth_strategy) {
    struct arraylist list;
    arraylist_init(&list, 1, &allocator);

    size_t prev_capacity = list.capacity;

    // Keep adding elements and check capacity growth
    for (int i = 0; i < 100; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;

        if (list.capacity > prev_capacity) {
            // Capacity should at least double
            ASSERT_GE(list.capacity, prev_capacity * 2);
            prev_capacity = list.capacity;
        }
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, stress_test_large_data) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    const int N = 1000;

    // Insert many elements
    for (int i = 0; i < N; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;
    }

    // Verify all elements
    ASSERT_EQ(list.size, N * sizeof(int));
    for (int i = 0; i < N; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), i);
    }

    // Remove every other element
    for (int i = N - 1; i >= 0; i -= 2) {
        arraylist_remove(&list, i * sizeof(int), sizeof(int));
    }

    // Verify remaining elements
    ASSERT_EQ(list.size, (N / 2) * sizeof(int));
    for (int i = 0; i < N / 2; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), i * 2);
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, multiple_data_types) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    // Add different sized data
    char* c_ptr = (char*)arraylist_push_back(&list, sizeof(char));
    *c_ptr = 'A';

    const int i = 42;
    void* i_ptr = arraylist_push_back(&list, sizeof(int));
    std::memcpy(i_ptr, &i, sizeof(int));

    const double d = 3.14;
    void* d_ptr = arraylist_push_back(&list, sizeof(double));
    std::memcpy(d_ptr, &d, sizeof(double));

    // Verify data
    ASSERT_EQ(*(char*)((char*)list.data), 'A');

    int i_val;
    std::memcpy(&i_val, (char*)list.data + sizeof(char), sizeof(int));
    ASSERT_EQ(i_val, 42);

    double d_val;
    std::memcpy(&d_val, (char*)list.data + sizeof(char) + sizeof(int), sizeof(double));
    ASSERT_EQ(d_val, 3.14);

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, insert_multiple_elements) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    // Add base elements [0, 1, 4, 5]
    int base[] = {0, 1, 4, 5};
    for (int i = 0; i < 4; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = base[i];
    }

    // Insert multiple elements [2, 3] at position 2
    int* ptr = (int*)arraylist_insert(&list, 2 * sizeof(int), 2 * sizeof(int));
    ptr[0] = 2;
    ptr[1] = 3;

    ASSERT_EQ(list.size, 6 * sizeof(int));
    for (int i = 0; i < 6; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), i);
    }

    arraylist_shrink_to(&list, 0);
}

TEST_F(arraylist_test, remove_multiple_elements) {
    struct arraylist list;
    arraylist_init(&list, 0, &allocator);

    // Add elements [0, 1, 2, 3, 4, 5]
    for (int i = 0; i < 6; ++i) {
        int* ptr = (int*)arraylist_push_back(&list, sizeof(int));
        *ptr = i;
    }

    // Remove elements at positions 2 and 3
    arraylist_remove(&list, 2 * sizeof(int), 2 * sizeof(int));

    ASSERT_EQ(list.size, 4 * sizeof(int));
    int expected[] = {0, 1, 4, 5};
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(arraylist_at(&list, int, i), expected[i]);
    }

    arraylist_shrink_to(&list, 0);
}
