#include <gtest/gtest.h>

#include "LibkcApi.h"

class LibkcDlTest : public ::testing::Test {
protected:
    static LibkcApi api;

    static void SetUpTestSuite() {
        api = load_api();
    }

    static void TearDownTestSuite() {
        unload_api(api);
    }
};

LibkcApi LibkcDlTest::api{};

TEST_F(LibkcDlTest, MemcpyCopiesBytes) {
    unsigned char src[5] = {1, 2, 3, 4, 5};
    unsigned char dst[5] = {0, 0, 0, 0, 0};

    void *ret = api.memcpy_ptr(dst, src, 5);
    EXPECT_EQ(ret, dst);
    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[1], 2);
    EXPECT_EQ(dst[2], 3);
    EXPECT_EQ(dst[3], 4);
    EXPECT_EQ(dst[4], 5);
}

TEST_F(LibkcDlTest, MemmoveHandlesOverlap) {
    char buf1[8] = "abcdef";
    char buf2[8] = "abcdef";

    api.memmove_ptr(buf1 + 1, buf1, 5);
    api.memmove_ptr(buf2, buf2 + 1, 5);

    EXPECT_STREQ(buf1, "aabcde");
    EXPECT_STREQ(buf2, "bcdeff");
}

TEST_F(LibkcDlTest, MemsetFillsRange) {
    char buf[6] = "abcde";
    void *ret = api.memset_ptr(buf + 1, 'x', 3);

    EXPECT_EQ(ret, buf + 1);
    EXPECT_STREQ(buf, "axxxe");
}

TEST_F(LibkcDlTest, MemcmpReturnsOrdering) {
    const unsigned char a[3] = {1, 2, 3};
    const unsigned char b[3] = {1, 2, 4};

    EXPECT_EQ(api.memcmp_ptr(a, a, 3), 0);
    EXPECT_LT(api.memcmp_ptr(a, b, 3), 0);
    EXPECT_GT(api.memcmp_ptr(b, a, 3), 0);
}

TEST_F(LibkcDlTest, StrlenCountsBytes) {
    EXPECT_EQ(api.strlen_ptr(""), static_cast<size_t>(0));
    EXPECT_EQ(api.strlen_ptr("opal"), static_cast<size_t>(4));
}

TEST_F(LibkcDlTest, StrspnMatchesPrefixSet) {
    EXPECT_EQ(api.strspn_ptr("   echo", " "), static_cast<size_t>(3));
    EXPECT_EQ(api.strspn_ptr("abc123", "abc"), static_cast<size_t>(3));
    EXPECT_EQ(api.strspn_ptr("xyz", "abc"), static_cast<size_t>(0));
}

TEST_F(LibkcDlTest, StrchrFindsFirstOccurrenceAndNullTerminator) {
    const char *s = "banana";

    char *found = api.strchr_ptr(s, 'n');
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found - s, 2);

    char *end = api.strchr_ptr(s, '\0');
    ASSERT_NE(end, nullptr);
    EXPECT_EQ(end - s, 6);

    EXPECT_EQ(api.strchr_ptr(s, 'x'), nullptr);
}

TEST_F(LibkcDlTest, StrnchrRespectsLimit) {
    const char *s = "banana";

    char *found = api.strnchr_ptr(s, 'n', 4);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found - s, 2);

    EXPECT_EQ(api.strnchr_ptr(s, 'n', 2), nullptr);
    EXPECT_EQ(api.strnchr_ptr(s, '\0', 3), nullptr);
    EXPECT_NE(api.strnchr_ptr(s, '\0', 7), nullptr);
}

TEST_F(LibkcDlTest, StrcmpReturnsOrdering) {
    EXPECT_EQ(api.strcmp_ptr("opal", "opal"), 0);
    EXPECT_LT(api.strcmp_ptr("opal", "opal-os"), 0);
    EXPECT_GT(api.strcmp_ptr("opalz", "opal"), 0);
}

TEST_F(LibkcDlTest, StrncmpRespectsLimit) {
    EXPECT_EQ(api.strncmp_ptr("opal", "opal-os", 4), 0);
    EXPECT_LT(api.strncmp_ptr("abc", "abd", 3), 0);
    EXPECT_GT(api.strncmp_ptr("abe", "abd", 3), 0);
    EXPECT_EQ(api.strncmp_ptr("abc", "xyz", 0), 0);
}

TEST_F(LibkcDlTest, StrcpyCopiesIncludingTerminator) {
    char dst[16];
    char *ret = api.strcpy_ptr(dst, "kernel");

    EXPECT_EQ(ret, dst);
    EXPECT_STREQ(dst, "kernel");
}

TEST_F(LibkcDlTest, StrncpyPadsWithZerosAndCanTruncate) {
    char dst1[8];
    for (size_t i = 0; i < sizeof(dst1); i++) {
        dst1[i] = '#';
    }
    char *ret1 = api.strncpy_ptr(dst1, "opal", 7);
    EXPECT_EQ(ret1, dst1);
    EXPECT_EQ(dst1[0], 'o');
    EXPECT_EQ(dst1[1], 'p');
    EXPECT_EQ(dst1[2], 'a');
    EXPECT_EQ(dst1[3], 'l');
    EXPECT_EQ(dst1[4], '\0');
    EXPECT_EQ(dst1[5], '\0');
    EXPECT_EQ(dst1[6], '\0');

    char dst2[4] = {'#', '#', '#', '#'};
    char *ret2 = api.strncpy_ptr(dst2, "kernel", 4);
    EXPECT_EQ(ret2, dst2);
    EXPECT_EQ(dst2[0], 'k');
    EXPECT_EQ(dst2[1], 'e');
    EXPECT_EQ(dst2[2], 'r');
    EXPECT_EQ(dst2[3], 'n');
}
