#include <opal/test.h>
#include <opal/utils/singlylist.h>

DEFINE_TEST(utils_singlylist_remove_after) {
    struct singlylist list = SINGLYLIST_EMPTY;
    struct singlylist_link first, middle, last;
    TEST_EXPECT_EQ(NULL, singlylist_pop_front(&list));

    singlylist_push_front(&list, &first);
    singlylist_insert_after(&first, &middle);
    singlylist_insert_after(&middle, &last);
    singlylist_remove_after(&first);

    TEST_EXPECT_EQ(&first, singlylist_head(&list));
    TEST_EXPECT_EQ(&last, first.next);
    TEST_EXPECT_EQ(&first, singlylist_pop_front(&list));
    TEST_EXPECT_EQ(&last, singlylist_pop_front(&list));
    TEST_EXPECT_EQ(NULL, singlylist_head(&list));
}
