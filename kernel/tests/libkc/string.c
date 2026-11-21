#include <opal/test.h>
#include <kc/string.h>

DEFINE_TEST(utils_memmove_overlap) {
    char buffer[] = "abcdef";
    memmove(buffer + 1, buffer, 5);
    TEST_EXPECT_STREQ("aabcde", buffer);
    memmove(buffer, buffer + 1, 6);
    TEST_EXPECT_STREQ("abcde", buffer);
}
