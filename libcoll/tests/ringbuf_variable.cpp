#include <gtest/gtest.h>

extern "C" {
#include <collections/ringbuf_variable.h>
}

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>

struct packet_header {
    uint16_t type;
    uint16_t flags;
    uint32_t seq;
};

struct testbuffer_variable {
    void* buffer;
    ringbuf_variable rb;

    explicit testbuffer_variable(size_t buffer_size) {
        buffer = malloc(buffer_size);
        if (!buffer) {
            throw std::bad_alloc();
        }
        ringbuf_variable_init(&rb, buffer, buffer_size, sizeof(packet_header), alignof(packet_header));
    }

    ~testbuffer_variable() {
        free(buffer);
    }

    ringbuf_variable* get() {
        return &rb;
    }
};

TEST(test_ringbuf_variable, push_pop_variable_record) {
    testbuffer_variable rb(256);

    packet_header in_header{ .type = 7, .flags = 0x12, .seq = 99 };
    const char payload[] = "opal";

    ASSERT_TRUE(ringbuf_variable_push(rb.get(), &in_header, payload, sizeof(payload)));

    packet_header out_header{};
    ringbuf_variable_data_view view{};
    ASSERT_TRUE(ringbuf_variable_pop(rb.get(), &out_header, &view));

    EXPECT_EQ(out_header.type, in_header.type);
    EXPECT_EQ(out_header.flags, in_header.flags);
    EXPECT_EQ(out_header.seq, in_header.seq);
    EXPECT_EQ(view.part1_size + view.part2_size, sizeof(payload));
    EXPECT_EQ(view.part2_size, 0u);
    EXPECT_EQ(memcmp(view.part1, payload, sizeof(payload)), 0);
}

TEST(test_ringbuf_variable, wraps_and_splits_data_in_two_parts) {
    testbuffer_variable rb(64);

    packet_header h0{ .type = 0, .flags = 0, .seq = 0 };
    packet_header h1{ .type = 1, .flags = 0, .seq = 1 };
    const char p0[] = "0123456789ABCDEFGHIJKLMN";
    const char p1[] = "abcdefghijklmnopqrst";

    ASSERT_TRUE(ringbuf_variable_push(rb.get(), &h0, p0, sizeof(p0)));

    packet_header out{};
    ringbuf_variable_data_view view{};

    ASSERT_TRUE(ringbuf_variable_pop(rb.get(), &out, &view));
    EXPECT_EQ(out.type, 0);
    EXPECT_EQ(view.part1_size + view.part2_size, sizeof(p0));
    EXPECT_EQ(view.part2_size, 0u);
    EXPECT_EQ(memcmp(view.part1, p0, sizeof(p0)), 0);

    ASSERT_TRUE(ringbuf_variable_push(rb.get(), &h1, p1, sizeof(p1)));

    ASSERT_TRUE(ringbuf_variable_pop(rb.get(), &out, &view));
    EXPECT_EQ(out.type, 1);
    EXPECT_EQ(view.part1_size + view.part2_size, sizeof(p1));
    EXPECT_GT(view.part1_size, 0u);
    EXPECT_GT(view.part2_size, 0u);
    EXPECT_EQ(view.part1_size + view.part2_size, sizeof(p1));
    EXPECT_EQ(memcmp(view.part1, p1, view.part1_size), 0);
    EXPECT_EQ(memcmp(view.part2, p1 + view.part1_size, view.part2_size), 0);
}

TEST(test_ringbuf_variable, pop_without_data_view) {
    testbuffer_variable rb(256);

    packet_header in_header{ .type = 8, .flags = 0, .seq = 10 };
    const char payload[] = "this payload is long";
    ASSERT_TRUE(ringbuf_variable_push(rb.get(), &in_header, payload, sizeof(payload)));

    packet_header out_header{};
    ASSERT_TRUE(ringbuf_variable_pop(rb.get(), &out_header, NULL));
    EXPECT_EQ(out_header.type, in_header.type);
}
