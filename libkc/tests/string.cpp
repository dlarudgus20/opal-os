#include "test-pch.h"

#include "LibkcTest.h"

TEST_F(LibkcTest, MemcpyCopiesBytes) {
    unsigned char src[5] = {1, 2, 3, 4, 5};
    unsigned char dst[5] = {0, 0, 0, 0, 0};

    void *ret = kc.memcpy(dst, src, 5);
    EXPECT_EQ(ret, dst);
    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[1], 2);
    EXPECT_EQ(dst[2], 3);
    EXPECT_EQ(dst[3], 4);
    EXPECT_EQ(dst[4], 5);
}

TEST_F(LibkcTest, MemmoveHandlesOverlap) {
    char buf1[8] = "abcdef";
    char buf2[8] = "abcdef";

    kc.memmove(buf1 + 1, buf1, 5);
    kc.memmove(buf2, buf2 + 1, 5);

    EXPECT_STREQ(buf1, "aabcde");
    EXPECT_STREQ(buf2, "bcdeff");
}

TEST_F(LibkcTest, MemsetFillsRange) {
    char buf[6] = "abcde";
    void *ret = kc.memset(buf + 1, 'x', 3);

    EXPECT_EQ(ret, buf + 1);
    EXPECT_STREQ(buf, "axxxe");
}

TEST_F(LibkcTest, MemcmpReturnsOrdering) {
    const unsigned char a[3] = {1, 2, 3};
    const unsigned char b[3] = {1, 2, 4};

    EXPECT_EQ(kc.memcmp(a, a, 3), 0);
    EXPECT_LT(kc.memcmp(a, b, 3), 0);
    EXPECT_GT(kc.memcmp(b, a, 3), 0);
}

TEST_F(LibkcTest, StrlenCountsBytes) {
    EXPECT_EQ(kc.strlen(""), static_cast<size_t>(0));
    EXPECT_EQ(kc.strlen("opal"), static_cast<size_t>(4));
}

TEST_F(LibkcTest, StrspnMatchesPrefixSet) {
    EXPECT_EQ(kc.strspn("   echo", " "), static_cast<size_t>(3));
    EXPECT_EQ(kc.strspn("abc123", "abc"), static_cast<size_t>(3));
    EXPECT_EQ(kc.strspn("xyz", "abc"), static_cast<size_t>(0));
}

TEST_F(LibkcTest, StrchrFindsFirstOccurrenceAndNullTerminator) {
    const char *s = "banana";

    char *found = kc.strchr(s, 'n');
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found - s, 2);

    char *end = kc.strchr(s, '\0');
    ASSERT_NE(end, nullptr);
    EXPECT_EQ(end - s, 6);

    EXPECT_EQ(kc.strchr(s, 'x'), nullptr);
}

TEST_F(LibkcTest, StrnchrRespectsLimit) {
    const char *s = "banana";

    char *found = kc.strnchr(s, 'n', 4);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found - s, 2);

    EXPECT_EQ(kc.strnchr(s, 'n', 2), nullptr);
    EXPECT_EQ(kc.strnchr(s, '\0', 3), nullptr);
    EXPECT_NE(kc.strnchr(s, '\0', 7), nullptr);
}

TEST_F(LibkcTest, StrcmpReturnsOrdering) {
    EXPECT_EQ(kc.strcmp("opal", "opal"), 0);
    EXPECT_LT(kc.strcmp("opal", "opal-os"), 0);
    EXPECT_GT(kc.strcmp("opalz", "opal"), 0);
}

TEST_F(LibkcTest, StrncmpRespectsLimit) {
    EXPECT_EQ(kc.strncmp("opal", "opal-os", 4), 0);
    EXPECT_LT(kc.strncmp("abc", "abd", 3), 0);
    EXPECT_GT(kc.strncmp("abe", "abd", 3), 0);
    EXPECT_EQ(kc.strncmp("abc", "xyz", 0), 0);
}

TEST_F(LibkcTest, StrcpyCopiesIncludingTerminator) {
    char dst[16];
    char *ret = kc.strcpy(dst, "kernel");

    EXPECT_EQ(ret, dst);
    EXPECT_STREQ(dst, "kernel");
}

TEST_F(LibkcTest, StrncpySizedCopiesAndTerminates) {
    char dst[8];
    for (size_t i = 0; i < sizeof(dst); i++) {
        dst[i] = '#';
    }

    kc.strncpy_sized(dst, sizeof(dst), "kernel", 3);
    EXPECT_STREQ(dst, "ker");
    EXPECT_EQ(dst[4], '#');
}

TEST_F(LibkcTest, StrncpySizedZeroCountWritesOnlyTerminator) {
    char dst[8];
    for (size_t i = 0; i < sizeof(dst); i++) {
        dst[i] = '#';
    }

    kc.strncpy_sized(dst, sizeof(dst), "kernel", 0);
    EXPECT_EQ(dst[0], '\0');
    EXPECT_EQ(dst[1], '#');
}

TEST_F(LibkcTest, StrncpySizedClampsToCapacityMinusOne) {
    char dst[5] = {'#', '#', '#', '#', '#'};

    kc.strncpy_sized(dst, sizeof(dst), "opal!", 8);
    EXPECT_STREQ(dst, "opal");
    EXPECT_EQ(dst[4], '\0');
}

TEST_F(LibkcTest, StrncpySizedReturnsWhenDestSizeIsZero) {
    char dst[3] = {'a', 'b', 'c'};

    kc.strncpy_sized(dst, 0, "xyz", 3);
    EXPECT_EQ(dst[0], 'a');
    EXPECT_EQ(dst[1], 'b');
    EXPECT_EQ(dst[2], 'c');
}

TEST_F(LibkcTest, StrncpySizedEmptySourceWithOneByteDest) {
    char dst[1] = {'#'};
    kc.strncpy_sized(dst, sizeof(dst), "", 8);
    EXPECT_EQ(dst[0], '\0');
}

TEST_F(LibkcTest, StrncpySizedCopiesUntilSourceTerminatorWhenNIsLarge) {
    char dst[16];
    for (size_t i = 0; i < sizeof(dst); i++) {
        dst[i] = '#';
    }

    kc.strncpy_sized(dst, sizeof(dst), "opal", 32);
    EXPECT_STREQ(dst, "opal");
    EXPECT_EQ(dst[5], '#');
}

TEST_F(LibkcTest, StrncpySizedAllowsExactFitOfNPlusTerminator) {
    char dst[5] = {'#', '#', '#', '#', '#'};
    kc.strncpy_sized(dst, sizeof(dst), "opal!", 4);
    EXPECT_STREQ(dst, "opal");
}

TEST_F(LibkcTest, StrncpySizedTruncatesWithoutPanic) {
    char dst[4] = {'#', '#', '#', '#'};

    kc.strncpy_sized(dst, sizeof(dst), "abcde", 5);
    EXPECT_EQ(dst[0], 'a');
    EXPECT_EQ(dst[1], 'b');
    EXPECT_EQ(dst[2], 'c');
    EXPECT_EQ(dst[3], '\0');
}

TEST_F(LibkcTest, StrcatAppendsAndReturnsDest) {
    char dst[16] = "opal";
    char *ret = kc.strcat(dst, "-os");

    EXPECT_EQ(ret, dst);
    EXPECT_STREQ(dst, "opal-os");
}

TEST_F(LibkcTest, StrncatRespectsCountAndTerminates) {
    char dst[16] = "op";
    char *ret = kc.strncat(dst, "alkernel", 3);

    EXPECT_EQ(ret, dst);
    EXPECT_STREQ(dst, "opalk");
}

TEST_F(LibkcTest, StrcatSizedAppendsWithinCapacity) {
    char dst[8] = "opa";
    kc.strcat_sized(dst, sizeof(dst), "l");
    EXPECT_STREQ(dst, "opal");
}

TEST_F(LibkcTest, StrcatSizedTruncatesSafely) {
    char dst[8] = "opal";
    kc.strcat_sized(dst, sizeof(dst), "-kernel");
    EXPECT_STREQ(dst, "opal-ke");
    EXPECT_EQ(dst[7], '\0');
}

TEST_F(LibkcTest, StrcatSizedNoTerminatorInDestLeavesBufferUntouched) {
    char dst[4] = {'a', 'b', 'c', 'd'};
    kc.strcat_sized(dst, sizeof(dst), "x");
    EXPECT_EQ(dst[0], 'a');
    EXPECT_EQ(dst[1], 'b');
    EXPECT_EQ(dst[2], 'c');
    EXPECT_EQ(dst[3], 'd');
}
