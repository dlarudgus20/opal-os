#include "test-pch.h"

#include "LibkcTest.h"

TEST_F(LibkcTest, KsnprintfWritesLiteralAndReturnsCount) {
    char buf[32];
    int ret = kc.ksnprintf(buf, sizeof(buf), "hello");

    EXPECT_EQ(ret, 5);
    EXPECT_STREQ(buf, "hello");
}

TEST_F(LibkcTest, KsnprintfTruncatesAndKeepsNullTerminator) {
    char buf[4] = {'#', '#', '#', '#'};
    int ret = kc.ksnprintf(buf, sizeof(buf), "abcdef");

    EXPECT_EQ(ret, 6);
    EXPECT_EQ(buf[0], 'a');
    EXPECT_EQ(buf[1], 'b');
    EXPECT_EQ(buf[2], 'c');
    EXPECT_EQ(buf[3], '\0');
}

TEST_F(LibkcTest, KsnprintfReturnsMinusOneOnInvalidInputs) {
    char buf[8];
    EXPECT_EQ(kc.ksnprintf(nullptr, 8, "abc"), -1);
    EXPECT_EQ(kc.ksnprintf(buf, 8, nullptr), -1);
    EXPECT_EQ(kc.ksnprintf(buf, 0, "abc"), 3);
}

TEST_F(LibkcTest, KsnprintfFormatsIntegerSpecifier) {
    char buf[32];
    int ret = kc.ksnprintf(buf, sizeof(buf), "x=%d", 42);

    EXPECT_EQ(ret, 4);
    EXPECT_STREQ(buf, "x=42");
}

TEST_F(LibkcTest, KsnprintfAppliesWidthAndPrecision) {
    char buf[32];
    int ret = kc.ksnprintf(buf, sizeof(buf), "[%08x][%.3s]", 0x2a, "opal");

    EXPECT_EQ(ret, 15);
    EXPECT_STREQ(buf, "[0000002a][opa]");
}

TEST_F(LibkcTest, KsnprintfIgnoresFloatingPointSpecifiers) {
    char buf[32];
    int ret = kc.ksnprintf(buf, sizeof(buf), "v=%f", 1.25);

    EXPECT_EQ(ret, 2);
    EXPECT_STREQ(buf, "v=");
}

TEST_F(LibkcTest, KsnprintfWidthWithSignTest) {
    char buf[32];
    int ret = kc.ksnprintf(buf, sizeof(buf), "[%+5d]", 7);

    EXPECT_EQ(ret, 7);
    EXPECT_STREQ(buf, "[   +7]");
}

TEST_F(LibkcTest, KsnprintfAltPrecisionTest) {
    char buf[32];
    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%.0d", 0), 0);
    EXPECT_STREQ(buf, "");
    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%.0o", 0), 0);
    EXPECT_STREQ(buf, "");
    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%#.0o", 0), 1);
    EXPECT_STREQ(buf, "0");
    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%.0d", 5), 1);
    EXPECT_STREQ(buf, "5");
    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%.0o", 5), 1);
    EXPECT_STREQ(buf, "5");
    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%.1d", 5), 1);
    EXPECT_STREQ(buf, "5");
    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%.1o", 5), 1);
    EXPECT_STREQ(buf, "5");
    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%#.1o", 0), 1);
    EXPECT_STREQ(buf, "0");
    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%#.1o", 5), 2);
    EXPECT_STREQ(buf, "05");

    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%8.4d", 32), 8);
    EXPECT_STREQ(buf, "    0032");
    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%#.4x", 10), 6);
    EXPECT_STREQ(buf, "0x000a");
    EXPECT_EQ(kc.ksnprintf(buf, sizeof(buf), "%#.3o", 5), 3);
    EXPECT_STREQ(buf, "005");
}
