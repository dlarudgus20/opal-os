#include <opal/test.h>
#include <opal/utils/linkedlist.h>

DEFINE_TEST(utils_linkedlist_remove_middle) {
    struct linkedlist list;
    struct linkedlist_link first, middle, last;
    linkedlist_init(&list);
    TEST_EXPECT_TRUE(linkedlist_is_empty(&list));

    linkedlist_push_back(&list, &first);
    linkedlist_push_back(&list, &middle);
    linkedlist_push_back(&list, &last);
    linkedlist_remove(&middle);

    TEST_EXPECT_EQ(&first, linkedlist_head(&list));
    TEST_EXPECT_EQ(&last, linkedlist_tail(&list));
    TEST_EXPECT_EQ(&last, first.next);
    TEST_EXPECT_EQ(&first, last.prev);
    TEST_EXPECT_EQ(linkedlist_nil(&list), first.prev);
    TEST_EXPECT_EQ(linkedlist_nil(&list), last.next);
}
