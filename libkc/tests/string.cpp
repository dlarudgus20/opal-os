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

TEST_F(LibkcTest, StrncpyPadsWithZerosAndCanTruncate) {
    char dst1[8];
    for (size_t i = 0; i < sizeof(dst1); i++) {
        dst1[i] = '#';
    }
    char *ret1 = kc.strncpy(dst1, "opal", 7);
    EXPECT_EQ(ret1, dst1);
    EXPECT_EQ(dst1[0], 'o');
    EXPECT_EQ(dst1[1], 'p');
    EXPECT_EQ(dst1[2], 'a');
    EXPECT_EQ(dst1[3], 'l');
    EXPECT_EQ(dst1[4], '\0');
    EXPECT_EQ(dst1[5], '\0');
    EXPECT_EQ(dst1[6], '\0');

    char dst2[4] = {'#', '#', '#', '#'};
    char *ret2 = kc.strncpy(dst2, "kernel", 4);
    EXPECT_EQ(ret2, dst2);
    EXPECT_EQ(dst2[0], 'k');
    EXPECT_EQ(dst2[1], 'e');
    EXPECT_EQ(dst2[2], 'r');
    EXPECT_EQ(dst2[3], 'n');
}
