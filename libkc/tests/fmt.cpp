#include <gtest/gtest.h>
#include "libkc.h"

TEST(LibkcDlTest, SnprintfSWritesLiteralAndReturnsCount) {
    char buf[32];
    int ret = kc_snprintf_s(buf, sizeof(buf), "hello");

    EXPECT_EQ(ret, 5);
    EXPECT_STREQ(buf, "hello");
}

TEST(LibkcDlTest, SnprintfSTruncatesAndKeepsNullTerminator) {
    char buf[4] = {'#', '#', '#', '#'};
    int ret = kc_snprintf_s(buf, sizeof(buf), "abcdef");

    EXPECT_EQ(ret, 6);
    EXPECT_EQ(buf[0], 'a');
    EXPECT_EQ(buf[1], 'b');
    EXPECT_EQ(buf[2], 'c');
    EXPECT_EQ(buf[3], '\0');
}

TEST(LibkcDlTest, SnprintfSReturnsMinusOneOnInvalidInputs) {
    char buf[8];
    EXPECT_EQ(kc_snprintf_s(nullptr, 8, "abc"), -1);
    EXPECT_EQ(kc_snprintf_s(buf, 8, nullptr), -1);
    EXPECT_EQ(kc_snprintf_s(buf, 0, "abc"), -1);
}

TEST(LibkcDlTest, SnprintfSFormatsIntegerSpecifier) {
    char buf[32];
    int ret = kc_snprintf_s(buf, sizeof(buf), "x=%d", 42);

    EXPECT_EQ(ret, 4);
    EXPECT_STREQ(buf, "x=42");
}

TEST(LibkcDlTest, SnprintfSAppliesWidthAndPrecision) {
    char buf[32];
    int ret = kc_snprintf_s(buf, sizeof(buf), "[%08x][%.3s]", 0x2a, "opal");

    EXPECT_EQ(ret, 15);
    EXPECT_STREQ(buf, "[0000002a][opa]");
}

TEST(LibkcDlTest, SnprintfSIgnoresFloatingPointSpecifiers) {
    char buf[32];
    int ret = kc_snprintf_s(buf, sizeof(buf), "v=%f", 1.25);

    EXPECT_EQ(ret, 2);
    EXPECT_STREQ(buf, "v=");
}

TEST(LibkcDlTest, SnprintfSWidthWithSignTest) {
    char buf[32];
    int ret = kc_snprintf_s(buf, sizeof(buf), "[%+5d]", 7);

    EXPECT_EQ(ret, 7);
    EXPECT_STREQ(buf, "[   +7]");
}

TEST(LibkcDlTest, SnprintfSAltPrecisionTest) {
    char buf[32];
    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%.0d", 0), 0);
    EXPECT_STREQ(buf, "");
    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%.0o", 0), 0);
    EXPECT_STREQ(buf, "");
    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%#.0o", 0), 1);
    EXPECT_STREQ(buf, "0");
    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%.0d", 5), 1);
    EXPECT_STREQ(buf, "5");
    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%.0o", 5), 1);
    EXPECT_STREQ(buf, "5");
    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%.1d", 5), 1);
    EXPECT_STREQ(buf, "5");
    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%.1o", 5), 1);
    EXPECT_STREQ(buf, "5");
    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%#.1o", 0), 1);
    EXPECT_STREQ(buf, "0");
    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%#.1o", 5), 2);
    EXPECT_STREQ(buf, "05");

    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%8.4d", 32), 8);
    EXPECT_STREQ(buf, "    0032");
    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%#.4x", 10), 6);
    EXPECT_STREQ(buf, "0x000a");
    EXPECT_EQ(kc_snprintf_s(buf, sizeof(buf), "%#.3o", 5), 3);
    EXPECT_STREQ(buf, "005");
}
