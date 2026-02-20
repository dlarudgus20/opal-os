#include "test-pch.h"

extern "C" {
#define restrict
#include <collections/singlylist.h>
}

#include <kc/stdlib.h>

// 테스트용 노드 구조체
struct test_node {
    struct singlylist_link link;
    int value;

    test_node(int val) : value(val) {
        link.next = nullptr;
    }
};

#define link_to_node(link_ptr) container_of(link_ptr, struct test_node, link)

class singlylist_test : public ::testing::Test {
protected:
    void SetUp() override {
        singlylist_init(&list);
        nodes.clear();
    }

    void TearDown() override {
        // 메모리 정리
        for (auto* node : nodes) {
            delete node;
        }
        nodes.clear();
    }

    struct test_node* create_node(int value) {
        auto* node = new test_node(value);
        nodes.push_back(node);
        return node;
    }

    std::vector<int> list_to_vector() {
        std::vector<int> result;
        singlylist_foreach(link, &list) {
            struct test_node* node = link_to_node(link);
            result.push_back(node->value);
        }
        return result;
    }

    size_t count_nodes() {
        size_t count = 0;
        singlylist_foreach(link, &list) {
            count++;
        }
        return count;
    }

    struct singlylist list;
    std::vector<struct test_node*> nodes;
};

TEST_F(singlylist_test, init_empty_list) {
    EXPECT_EQ(singlylist_head(&list), nullptr);
    EXPECT_NE(singlylist_before_head(&list), nullptr);
    EXPECT_EQ(singlylist_before_head(&list)->next, nullptr);
    EXPECT_EQ(count_nodes(), 0);
}

TEST_F(singlylist_test, push_front_single_element) {
    struct test_node* node = create_node(42);

    singlylist_push_front(&list, &node->link);

    EXPECT_EQ(singlylist_head(&list), &node->link);
    EXPECT_EQ(count_nodes(), 1);

    auto values = list_to_vector();
    EXPECT_EQ(values, std::vector<int>({42}));
}

TEST_F(singlylist_test, push_front_multiple_elements) {
    std::vector<int> input_values = {1, 2, 3, 4, 5};

    // 순서대로 push_front
    for (int value : input_values) {
        struct test_node* node = create_node(value);
        singlylist_push_front(&list, &node->link);
    }

    EXPECT_EQ(count_nodes(), 5);

    // push_front는 역순으로 삽입됨
    auto values = list_to_vector();
    std::vector<int> expected = {5, 4, 3, 2, 1};
    EXPECT_EQ(values, expected);
}

TEST_F(singlylist_test, pop_front_empty_list) {
    struct singlylist_link* popped = singlylist_pop_front(&list);
    EXPECT_EQ(popped, nullptr);
    EXPECT_EQ(count_nodes(), 0);
}

TEST_F(singlylist_test, pop_front_single_element) {
    struct test_node* node = create_node(42);
    singlylist_push_front(&list, &node->link);

    struct singlylist_link* popped = singlylist_pop_front(&list);

    EXPECT_EQ(popped, &node->link);
    EXPECT_EQ(singlylist_head(&list), nullptr);
    EXPECT_EQ(count_nodes(), 0);

    struct test_node* popped_node = link_to_node(popped);
    EXPECT_EQ(popped_node->value, 42);
}

TEST_F(singlylist_test, pop_front_multiple_elements) {
    std::vector<int> input_values = {1, 2, 3};

    for (int value : input_values) {
        struct test_node* node = create_node(value);
        singlylist_push_front(&list, &node->link);
    }

    // push_front로 삽입했으므로 순서는 [3, 2, 1]

    // 첫 번째 pop
    struct singlylist_link* popped1 = singlylist_pop_front(&list);
    EXPECT_NE(popped1, nullptr);
    EXPECT_EQ(link_to_node(popped1)->value, 3);
    EXPECT_EQ(count_nodes(), 2);

    // 두 번째 pop
    struct singlylist_link* popped2 = singlylist_pop_front(&list);
    EXPECT_NE(popped2, nullptr);
    EXPECT_EQ(link_to_node(popped2)->value, 2);
    EXPECT_EQ(count_nodes(), 1);

    // 세 번째 pop
    struct singlylist_link* popped3 = singlylist_pop_front(&list);
    EXPECT_NE(popped3, nullptr);
    EXPECT_EQ(link_to_node(popped3)->value, 1);
    EXPECT_EQ(count_nodes(), 0);

    // 네 번째 pop (빈 리스트)
    struct singlylist_link* popped4 = singlylist_pop_front(&list);
    EXPECT_EQ(popped4, nullptr);
}

TEST_F(singlylist_test, insert_after_at_head) {
    struct test_node* node1 = create_node(1);
    struct test_node* node2 = create_node(2);

    singlylist_push_front(&list, &node1->link);
    singlylist_insert_after(&node1->link, &node2->link);

    auto values = list_to_vector();
    EXPECT_EQ(values, std::vector<int>({1, 2}));
    EXPECT_EQ(count_nodes(), 2);
}

TEST_F(singlylist_test, insert_after_at_middle) {
    struct test_node* node1 = create_node(1);
    struct test_node* node2 = create_node(2);
    struct test_node* node3 = create_node(3);

    singlylist_push_front(&list, &node1->link);
    singlylist_insert_after(&node1->link, &node3->link);  // [1, 3]
    singlylist_insert_after(&node1->link, &node2->link);  // [1, 2, 3]

    auto values = list_to_vector();
    EXPECT_EQ(values, std::vector<int>({1, 2, 3}));
    EXPECT_EQ(count_nodes(), 3);
}

TEST_F(singlylist_test, insert_after_before_head) {
    struct test_node* node1 = create_node(1);
    struct test_node* node2 = create_node(2);

    // before_head 다음에 삽입 (push_front와 동일)
    singlylist_insert_after(singlylist_before_head(&list), &node1->link);
    singlylist_insert_after(singlylist_before_head(&list), &node2->link);

    auto values = list_to_vector();
    EXPECT_EQ(values, std::vector<int>({2, 1}));
    EXPECT_EQ(count_nodes(), 2);
}

TEST_F(singlylist_test, remove_after_single_element) {
    struct test_node* node = create_node(42);
    singlylist_push_front(&list, &node->link);

    singlylist_remove_after(singlylist_before_head(&list));

    EXPECT_EQ(singlylist_head(&list), nullptr);
    EXPECT_EQ(count_nodes(), 0);
}

TEST_F(singlylist_test, remove_after_multiple_elements) {
    struct test_node* node1 = create_node(1);
    struct test_node* node2 = create_node(2);
    struct test_node* node3 = create_node(3);

    singlylist_push_front(&list, &node1->link);
    singlylist_insert_after(&node1->link, &node2->link);
    singlylist_insert_after(&node2->link, &node3->link);
    // 리스트: [1, 2, 3]

    // 두 번째 노드 제거 (node1 다음)
    singlylist_remove_after(&node1->link);

    auto values = list_to_vector();
    EXPECT_EQ(values, std::vector<int>({1, 3}));
    EXPECT_EQ(count_nodes(), 2);
}

TEST_F(singlylist_test, remove_after_head) {
    struct test_node* node1 = create_node(1);
    struct test_node* node2 = create_node(2);
    struct test_node* node3 = create_node(3);

    singlylist_push_front(&list, &node1->link);
    singlylist_insert_after(&node1->link, &node2->link);
    singlylist_insert_after(&node2->link, &node3->link);
    // 리스트: [1, 2, 3]

    // 첫 번째 노드 제거 (before_head 다음)
    singlylist_remove_after(singlylist_before_head(&list));

    auto values = list_to_vector();
    EXPECT_EQ(values, std::vector<int>({2, 3}));
    EXPECT_EQ(count_nodes(), 2);
}

TEST_F(singlylist_test, foreach_macro_empty_list) {
    std::vector<int> values;

    singlylist_foreach(link, &list) {
        struct test_node* node = link_to_node(link);
        values.push_back(node->value);
    }

    EXPECT_TRUE(values.empty());
}

TEST_F(singlylist_test, foreach_macro_multiple_elements) {
    std::vector<int> input_values = {1, 2, 3, 4, 5};

    for (int value : input_values) {
        struct test_node* node = create_node(value);
        singlylist_push_front(&list, &node->link);
    }

    std::vector<int> values;
    singlylist_foreach(link, &list) {
        struct test_node* node = link_to_node(link);
        values.push_back(node->value);
    }

    std::vector<int> expected = {5, 4, 3, 2, 1};  // push_front의 역순
    EXPECT_EQ(values, expected);
}

TEST_F(singlylist_test, foreach_2_macro_empty_list) {
    std::vector<std::pair<bool, int>> values;  // (has_before, value)

    singlylist_foreach_2(before, ptr, &list) {
        struct test_node* node = link_to_node(ptr);
        bool has_before = (before != singlylist_before_head(&list));
        values.push_back({has_before, node->value});
    }

    EXPECT_TRUE(values.empty());
}

TEST_F(singlylist_test, foreach_2_macro_multiple_elements) {
    std::vector<int> input_values = {1, 2, 3};

    for (int value : input_values) {
        struct test_node* node = create_node(value);
        singlylist_push_front(&list, &node->link);
    }
    // 리스트: [3, 2, 1]

    std::vector<std::pair<bool, int>> values;
    singlylist_foreach_2(before, ptr, &list) {
        struct test_node* node = link_to_node(ptr);
        bool has_before = (before != singlylist_before_head(&list));
        values.push_back({has_before, node->value});
    }

    std::vector<std::pair<bool, int>> expected = {
        {false, 3},  // 첫 번째 노드, before는 dummy
        {true, 2},   // 두 번째 노드, before는 첫 번째 노드
        {true, 1}    // 세 번째 노드, before는 두 번째 노드
    };
    EXPECT_EQ(values, expected);
}

TEST_F(singlylist_test, foreach_2_macro_remove_during_iteration) {
    struct test_node* node1 = create_node(1);
    struct test_node* node2 = create_node(2);
    struct test_node* node3 = create_node(3);
    struct test_node* node4 = create_node(4);

    singlylist_push_front(&list, &node1->link);
    singlylist_insert_after(&node1->link, &node2->link);
    singlylist_insert_after(&node2->link, &node3->link);
    singlylist_insert_after(&node3->link, &node4->link);
    // 리스트: [1, 2, 3, 4]

    // 짝수 값 노드들 제거
    singlylist_foreach_2(before, ptr, &list) {
        struct test_node* node = link_to_node(ptr);
        if (node->value % 2 == 0) {
            singlylist_remove_after(before);
            ptr = before;  // 다음 반복을 위해 포인터 조정
        }
    }

    auto values = list_to_vector();
    EXPECT_EQ(values, std::vector<int>({1, 3}));
    EXPECT_EQ(count_nodes(), 2);
}

TEST_F(singlylist_test, stress_test_large_list) {
    const int N = 1000;

    // 대량 삽입
    for (int i = 0; i < N; ++i) {
        struct test_node* node = create_node(i);
        singlylist_push_front(&list, &node->link);
    }

    EXPECT_EQ(count_nodes(), N);

    // 순회 검증
    int expected_value = N - 1;
    singlylist_foreach(link, &list) {
        struct test_node* node = link_to_node(link);
        EXPECT_EQ(node->value, expected_value);
        expected_value--;
    }

    // 절반 제거
    for (int i = 0; i < N / 2; ++i) {
        struct singlylist_link* popped = singlylist_pop_front(&list);
        EXPECT_NE(popped, nullptr);
    }

    EXPECT_EQ(count_nodes(), N - N / 2);
}

TEST_F(singlylist_test, alternating_push_pop) {
    std::vector<int> pushed_values;
    std::vector<int> popped_values;

    // 교대로 push와 pop 수행
    for (int i = 0; i < 10; ++i) {
        // 2개 push
        for (int j = 0; j < 2; ++j) {
            int value = i * 2 + j;
            struct test_node* node = create_node(value);
            singlylist_push_front(&list, &node->link);
            pushed_values.push_back(value);
        }

        // 1개 pop
        struct singlylist_link* popped = singlylist_pop_front(&list);
        if (popped) {
            struct test_node* node = link_to_node(popped);
            popped_values.push_back(node->value);
        }
    }

    // 최종적으로 리스트에 10개 요소가 남아야 함
    EXPECT_EQ(count_nodes(), 10);
    EXPECT_EQ(popped_values.size(), 10);
}

TEST_F(singlylist_test, complex_insertion_pattern) {
    struct test_node* node1 = create_node(1);
    struct test_node* node3 = create_node(3);
    struct test_node* node5 = create_node(5);

    // 홀수들을 먼저 삽입
    singlylist_push_front(&list, &node1->link);
    singlylist_insert_after(&node1->link, &node3->link);
    singlylist_insert_after(&node3->link, &node5->link);
    // 리스트: [1, 3, 5]

    // 짝수들을 사이사이에 삽입
    struct test_node* node2 = create_node(2);
    struct test_node* node4 = create_node(4);

    singlylist_insert_after(&node1->link, &node2->link);  // [1, 2, 3, 5]
    singlylist_insert_after(&node3->link, &node4->link);  // [1, 2, 3, 4, 5]

    auto values = list_to_vector();
    EXPECT_EQ(values, std::vector<int>({1, 2, 3, 4, 5}));
    EXPECT_EQ(count_nodes(), 5);
}

TEST_F(singlylist_test, list_reversal_using_foreach_2) {
    // 원본 리스트 생성: [1, 2, 3, 4, 5]
    for (int i = 5; i >= 1; --i) {  // 역순으로 push_front
        struct test_node* node = create_node(i);
        singlylist_push_front(&list, &node->link);
    }

    // 새로운 리스트에 역순으로 이동
    struct singlylist reversed_list;
    singlylist_init(&reversed_list);

    singlylist_foreach_2(before, ptr, &list) {
        // 현재 노드를 제거하고 새 리스트의 앞에 삽입
        singlylist_remove_after(before);
        struct test_node* node = link_to_node(ptr);
        singlylist_push_front(&reversed_list, &node->link);
        ptr = before;  // 다음 반복을 위해 포인터 조정
    }

    // 원본 리스트는 비어야 함
    EXPECT_EQ(count_nodes(), 0);

    // 역순 리스트 검증
    std::vector<int> reversed_values;
    singlylist_foreach(link, &reversed_list) {
        struct test_node* node = link_to_node(link);
        reversed_values.push_back(node->value);
    }

    EXPECT_EQ(reversed_values, std::vector<int>({5, 4, 3, 2, 1}));
}

// Death tests for error conditions
TEST_F(singlylist_test, remove_after_null_death) {
    struct test_node* node = create_node(42);
    singlylist_push_front(&list, &node->link);

    // 마지막 노드 다음을 제거하려고 시도 (assertion 실패해야 함)
    EXPECT_DEATH(singlylist_remove_after(&node->link), ".*");
}

TEST_F(singlylist_test, remove_after_empty_list_death) {
    // 빈 리스트에서 before_head 다음을 제거하려고 시도
    EXPECT_DEATH(singlylist_remove_after(singlylist_before_head(&list)), ".*");
}
