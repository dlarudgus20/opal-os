#include "test-pch.h"

extern "C" {
#define restrict
#include <collections/linkedlist.h>
};

TEST(linkedlist_test, empty_after_init) {
    linkedlist list;
    linkedlist_init(&list);
    ASSERT_EQ(linkedlist_head(&list), linkedlist_tail(&list));
}

TEST(linkedlist_test, push_back_single) {
    linkedlist list;
    linkedlist_link link;
    linkedlist_init(&list);
    linkedlist_push_back(&list, &link);
    ASSERT_EQ(linkedlist_head(&list), &link);
    ASSERT_EQ(linkedlist_tail(&list), &link);
    ASSERT_EQ(link.next, link.prev);
}

TEST(linkedlist_test, push_back_multiple) {
    linkedlist list;
    linkedlist_link link1, link2;
    linkedlist_init(&list);
    linkedlist_push_back(&list, &link1);
    linkedlist_push_back(&list, &link2);
    ASSERT_EQ(linkedlist_head(&list), &link1);
    ASSERT_EQ(linkedlist_tail(&list), &link2);
    ASSERT_EQ(link1.next, &link2);
    ASSERT_EQ(link1.prev, linkedlist_nil(&list));
    ASSERT_EQ(link2.next, linkedlist_nil(&list));
    ASSERT_EQ(link2.prev, &link1);
}

TEST(linkedlist_test, remove_only_element) {
    linkedlist list;
    linkedlist_link link;
    linkedlist_init(&list);
    linkedlist_push_back(&list, &link);
    linkedlist_remove(&link);
    ASSERT_EQ(linkedlist_head(&list), linkedlist_nil(&list));
    ASSERT_EQ(linkedlist_tail(&list), linkedlist_nil(&list));
}

TEST(linkedlist_test, remove_head) {
    linkedlist list;
    linkedlist_link link1, link2;
    linkedlist_init(&list);
    linkedlist_push_back(&list, &link1);
    linkedlist_push_back(&list, &link2);
    linkedlist_remove(&link1);
    ASSERT_EQ(linkedlist_head(&list), &link2);
    ASSERT_EQ(linkedlist_tail(&list), &link2);
    ASSERT_EQ(link2.prev, linkedlist_nil(&list));
    ASSERT_EQ(link2.next, linkedlist_nil(&list));
}

TEST(linkedlist_test, remove_tail) {
    linkedlist list;
    linkedlist_link link1, link2;
    linkedlist_init(&list);
    linkedlist_push_back(&list, &link1);
    linkedlist_push_back(&list, &link2);
    linkedlist_remove(&link2);
    ASSERT_EQ(linkedlist_head(&list), &link1);
    ASSERT_EQ(linkedlist_tail(&list), &link1);
    ASSERT_EQ(link1.prev, linkedlist_nil(&list));
    ASSERT_EQ(link1.next, linkedlist_nil(&list));
}

TEST(linkedlist_test, remove_middle) {
    linkedlist list;
    linkedlist_link link1, link2, link3;
    linkedlist_init(&list);
    linkedlist_push_back(&list, &link1);
    linkedlist_push_back(&list, &link2);
    linkedlist_push_back(&list, &link3);
    linkedlist_remove(&link2);
    ASSERT_EQ(linkedlist_head(&list), &link1);
    ASSERT_EQ(linkedlist_tail(&list), &link3);
    ASSERT_EQ(link1.prev, linkedlist_nil(&list));
    ASSERT_EQ(link1.next, &link3);
    ASSERT_EQ(link3.prev, &link1);
    ASSERT_EQ(link3.next, linkedlist_nil(&list));
}

TEST(linkedlist_test, iterate_forward) {
    linkedlist list;
    linkedlist_link link1, link2, link3;
    linkedlist_init(&list);
    linkedlist_push_back(&list, &link1);
    linkedlist_push_back(&list, &link2);
    linkedlist_push_back(&list, &link3);

    int count = 0;
    for (linkedlist_link* current = linkedlist_head(&list);
        !linkedlist_is_nil(&list, current);
        current = current->next) {
        count++;
    }

    ASSERT_EQ(count, 3);
}

TEST(linkedlist_test, iterate_backward) {
    linkedlist list;
    linkedlist_link link1, link2, link3;
    linkedlist_init(&list);
    linkedlist_push_back(&list, &link1);
    linkedlist_push_back(&list, &link2);
    linkedlist_push_back(&list, &link3);

    int count = 0;
    for (linkedlist_link* current = linkedlist_tail(&list);
        !linkedlist_is_nil(&list, current);
        current = current->prev) {
        count++;
    }

    ASSERT_EQ(count, 3);
}

TEST(linkedlist_test, pop_front_single_element) {
    linkedlist list;
    linkedlist_link link;
    linkedlist_init(&list);
    linkedlist_push_back(&list, &link);

    linkedlist_link* popped = linkedlist_pop_front(&list);
    ASSERT_EQ(popped, &link);
    ASSERT_EQ(linkedlist_head(&list), linkedlist_nil(&list));
    ASSERT_EQ(linkedlist_tail(&list), linkedlist_nil(&list));
}

TEST(linkedlist_test, pop_back_single_element) {
    linkedlist list;
    linkedlist_link link;
    linkedlist_init(&list);
    linkedlist_push_back(&list, &link);

    linkedlist_link* popped = linkedlist_pop_back(&list);
    ASSERT_EQ(popped, &link);
    ASSERT_EQ(linkedlist_head(&list), linkedlist_nil(&list));
    ASSERT_EQ(linkedlist_tail(&list), linkedlist_nil(&list));
}

TEST(linkedlist_test, pop_front_zero_element) {
    linkedlist list;
    linkedlist_init(&list);

    linkedlist_link* popped = linkedlist_pop_front(&list);
    ASSERT_EQ(popped, nullptr);
    ASSERT_EQ(linkedlist_head(&list), linkedlist_nil(&list));
    ASSERT_EQ(linkedlist_tail(&list), linkedlist_nil(&list));
}

TEST(linkedlist_test, pop_back_zero_element) {
    linkedlist list;
    linkedlist_init(&list);

    linkedlist_link* popped = linkedlist_pop_back(&list);
    ASSERT_EQ(popped, nullptr);
    ASSERT_EQ(linkedlist_head(&list), linkedlist_nil(&list));
    ASSERT_EQ(linkedlist_tail(&list), linkedlist_nil(&list));
}
