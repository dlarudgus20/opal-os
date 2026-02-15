#include <gtest/gtest.h>

#include <array>
#include <cstring>

extern "C" {
#define restrict
#include <opal/klog.h>
}

static void drain_klog(void) {
    struct klog_record_header header = {};
    std::array<char, KLOG_MAX_MSGLEN + 1> msg = {};

    uint32_t guard = 0;
    while (klog_read(&header, msg.data(), msg.size())) {
        guard++;
        ASSERT_LT(guard, 1u << 20);
    }
}

TEST(KlogTest, WriteReadSingleRecord) {
    drain_klog();

    constexpr char kMsg[] = "hello";
    klog_write(KLOG_INFO, kMsg, static_cast<uint16_t>(std::strlen(kMsg)));

    struct klog_record_header header = {};
    std::array<char, 16> out = {};
    ASSERT_TRUE(klog_read(&header, out.data(), out.size()));
    EXPECT_EQ(header.msglen, std::strlen(kMsg));
    EXPECT_EQ(header.level, KLOG_INFO);
    EXPECT_STREQ(out.data(), kMsg);

    ASSERT_FALSE(klog_read(&header, out.data(), out.size()));
}

TEST(KlogTest, InvalidLevelIsClamped) {
    drain_klog();

    constexpr char kMsg[] = "level clamp";
    klog_write(KLOG_LEVEL_COUNT + 10, kMsg, static_cast<uint16_t>(std::strlen(kMsg)));

    struct klog_record_header header = {};
    std::array<char, 32> out = {};
    ASSERT_TRUE(klog_read(&header, out.data(), out.size()));
    EXPECT_EQ(header.level, KLOG_DEBUG);
    EXPECT_STREQ(out.data(), kMsg);
}

TEST(KlogTest, ReadTruncatesAndTerminatesOutput) {
    drain_klog();

    constexpr char kMsg[] = "abcdef";
    klog_write(KLOG_NOTICE, kMsg, static_cast<uint16_t>(std::strlen(kMsg)));

    struct klog_record_header header = {};
    std::array<char, 4> out = {};
    ASSERT_TRUE(klog_read(&header, out.data(), out.size()));
    EXPECT_EQ(header.msglen, std::strlen(kMsg));
    EXPECT_STREQ(out.data(), "abc");
}

TEST(KlogTest, FormatWritesRecord) {
    drain_klog();

    const int written = klog_format(KLOG_WARNING, "x=%d y=%s", __FILE__, __func__, __LINE__, 42, "ok");
    ASSERT_GT(written, 0);

    struct klog_record_header header = {};
    std::array<char, 32> out = {};
    ASSERT_TRUE(klog_read(&header, out.data(), out.size()));
    EXPECT_EQ(header.level, KLOG_WARNING);
    EXPECT_STREQ(out.data(), "x=42 y=ok");
}

TEST(KlogDeathTest, ReadRejectsNullHeaderOut) {
    std::array<char, 8> out = {};
    EXPECT_DEATH({
        klog_read(nullptr, out.data(), out.size());
    }, "klog_read requires a non-NULL header_out");
}

TEST(KlogDeathTest, ReadRejectsInvalidMsgBuffer) {
    struct klog_record_header header = {};
    EXPECT_DEATH({
        klog_read(&header, nullptr, 8);
    },"klog_read requires a non-empty msg_out buffer");

    std::array<char, 1> out = {};
    EXPECT_DEATH({
        klog_read(&header, out.data(), 0);
    }, "klog_read requires a non-empty msg_out buffer");
}
