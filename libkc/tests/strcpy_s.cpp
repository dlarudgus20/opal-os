#include "test-pch.h"

#include "LibkcTest.h"

TEST_F(LibkcTest, StrcpySAndStrncpySBasicBehavior) {
    char dst1[16];
    for (size_t i = 0; i < sizeof(dst1); i++) {
        dst1[i] = '#';
    }

    kc.strcpy_s(dst1, sizeof(dst1), "opal");
    EXPECT_STREQ(dst1, "opal");

    char dst2[8];
    for (size_t i = 0; i < sizeof(dst2); i++) {
        dst2[i] = '#';
    }

    kc.strncpy_s(dst2, sizeof(dst2), "kernel", 3);
    EXPECT_STREQ(dst2, "ker");
}

TEST_F(LibkcTest, StrcpySEmptySourceWithOneByteDest) {
    char dst[1] = {'#'};
    kc.strcpy_s(dst, sizeof(dst), "");
    EXPECT_EQ(dst[0], '\0');
}

TEST_F(LibkcTest, StrncpySZeroCountWritesOnlyTerminator) {
    char dst[8];
    for (size_t i = 0; i < sizeof(dst); i++) {
        dst[i] = '#';
    }

    kc.strncpy_s(dst, sizeof(dst), "kernel", 0);
    EXPECT_EQ(dst[0], '\0');
    EXPECT_EQ(dst[1], '#');
}

TEST_F(LibkcTest, StrncpySCopiesUntilSourceTerminatorWhenNIsLarge) {
    char dst[16];
    for (size_t i = 0; i < sizeof(dst); i++) {
        dst[i] = '#';
    }

    kc.strncpy_s(dst, sizeof(dst), "opal", 32);
    EXPECT_STREQ(dst, "opal");
    EXPECT_EQ(dst[5], '#');
}

TEST_F(LibkcTest, StrcpySPanicsOnTruncation) {
    EXPECT_DEATH({
        char dst[4];
        kc.strcpy_s(dst, sizeof(dst), "abcd");
    }, "truncation occur");
}

TEST_F(LibkcTest, StrncpySPanicsOnTruncation) {
    EXPECT_DEATH({
        char dst[4];
        kc.strncpy_s(dst, sizeof(dst), "abcde", 5);
    }, "truncation occur");
}

TEST_F(LibkcTest, StrcpySPanicsOnOverlap) {
    EXPECT_DEATH({
        char buf[16] = "kernel";
        kc.strcpy_s(buf + 1, sizeof(buf) - 1, buf);
    }, "overlap occur");
}

TEST_F(LibkcTest, StrncpySPanicsOnOverlap) {
    EXPECT_DEATH({
        char buf[16] = "kernel";
        kc.strncpy_s(buf, sizeof(buf), buf + 1, 4);
    }, "overlap occur");
}

TEST_F(LibkcTest, StrncpySAllowsExactFitOfNPlusTerminator) {
    char dst[5] = {'#', '#', '#', '#', '#'};
    kc.strncpy_s(dst, sizeof(dst), "opal!", 4);
    EXPECT_STREQ(dst, "opal");
}

TEST_F(LibkcTest, StrcpySPanicsWhenDestSizeIsZero) {
    EXPECT_DEATH({
        char dst[1] = {'#'};
        kc.strcpy_s(dst, 0, "a");
    }, "destsz > 0");
}

TEST_F(LibkcTest, StrncpySPanicsWhenDestSizeIsZero) {
    EXPECT_DEATH({
        char dst[1] = {'#'};
        kc.strncpy_s(dst, 0, "a", 1);
    }, "destsz > 0");
}
