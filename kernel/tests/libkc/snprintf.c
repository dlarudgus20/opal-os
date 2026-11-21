#include <opal/test.h>
#include <kc/stdio.h>

DEFINE_TEST(utils_snprintf) {
    char buffer[16];
    TEST_ASSERT_EQ(6, ksnprintf(buffer, sizeof(buffer), "%s-%u", "opal", 7u));
    TEST_EXPECT_STREQ("opal-7", buffer);
    TEST_EXPECT_EQ(6, ksnprintf(buffer, 5, "%s-%u", "opal", 7u));
    TEST_EXPECT_STREQ("opal", buffer);
}
