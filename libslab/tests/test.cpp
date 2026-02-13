#include <gtest/gtest.h>

extern "C" {
#include <slab/slab.h>
}

TEST(SlabTest, BasicTest) {
    const char* result = slab_test();
    EXPECT_STREQ(result, "slab test passed\n");
}
