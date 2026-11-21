#ifndef OPAL_TEST_H
#define OPAL_TEST_H

#include <stdint.h>

#include <kc/attributes.h>
#include <kc/string.h>
#include <kc/kassert.h>

#define PROTOTYPE_TEST(name) \
    static void test_func__##name (void)

void test_run(void);
void test_run_heavy(void);
void test_abort(void);

#ifdef OPAL_TEST

struct test_info {
    void (*fn)(void);
    const char *item;
};

#if !__has_attribute(section)
#error "kernel test requires [[gnu::section]]"
#elif !__has_attribute(used)
#error "kernel test requires [[gnu::used]]"
#endif

#define DEFINE_TEST(name) \
    PROTOTYPE_TEST(name); \
    static const struct test_info test_item__##name = { \
        .fn = test_func__##name , \
        .item = #name , \
    }; \
    static const struct test_info *const test_ptr__##name \
    [[maybe_unused, gnu::used, gnu::section(".test")]] = &test_item__##name ; \
    PROTOTYPE_TEST(name)

void test_condition_failed(const char *check, const char *expr, const char *file, unsigned line);
void test_expect_eq_u64_failed(const char *expected_expr, const char *actual_expr,
    uint64_t expected, uint64_t actual, const char *file, unsigned line);
void test_expect_streq_failed(const char *expected_expr, const char *actual_expr,
    const char *expected, const char *actual, const char *file, unsigned line);

#define TEST_EXPECT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            test_condition_failed("EXPECT_TRUE", #expr, __FILE__, __LINE__); \
        } \
    } while (0)

#define TEST_EXPECT_FALSE(expr) \
    do { \
        if (expr) { \
            test_condition_failed("EXPECT_FALSE", #expr, __FILE__, __LINE__); \
        } \
    } while (0)

#define TEST_EXPECT_EQ(expected, actual) \
    do { \
        const uint64_t _test_expected = (uint64_t)(expected); \
        const uint64_t _test_actual = (uint64_t)(actual); \
        if (_test_expected != _test_actual) { \
            test_expect_eq_u64_failed(#expected, #actual, _test_expected, _test_actual, __FILE__, __LINE__); \
        } \
    } while (0)

#define TEST_EXPECT_STREQ(expected, actual) \
    do { \
        const char *const _test_expected = (expected); \
        const char *const _test_actual = (actual); \
        if ((_test_expected == 0 && _test_actual != 0) || \
            (_test_expected != 0 && _test_actual == 0) || \
            (_test_expected != 0 && _test_actual != 0 && strcmp(_test_expected, _test_actual) != 0)) { \
            test_expect_streq_failed(#expected, #actual, _test_expected, _test_actual, __FILE__, __LINE__); \
        } \
    } while (0)

#define TEST_ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            test_condition_failed("ASSERT_TRUE", #expr, __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_FALSE(expr) \
    do { \
        if (expr) { \
            test_condition_failed("ASSERT_FALSE", #expr, __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(expected, actual) \
    do { \
        const uint64_t _test_expected = (uint64_t)(expected); \
        const uint64_t _test_actual = (uint64_t)(actual); \
        if (_test_expected != _test_actual) { \
            test_expect_eq_u64_failed(#expected, #actual, _test_expected, _test_actual, __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_STREQ(expected, actual) \
    do { \
        const char *const _test_expected = (expected); \
        const char *const _test_actual = (actual); \
        if ((_test_expected == 0 && _test_actual != 0) || \
            (_test_expected != 0 && _test_actual == 0) || \
            (_test_expected != 0 && _test_actual != 0 && strcmp(_test_expected, _test_actual) != 0)) { \
            test_expect_streq_failed(#expected, #actual, _test_expected, _test_actual, __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#else /* OPAL_TEST */

#define DEFINE_TEST(name)                   [[maybe_unused]] PROTOTYPE_TEST(name)
#define TEST_PANIC()                        panic("This executable is not configured for tests.")
#define TEST_EXPECT_TRUE(expr)              do { TEST_PANIC(); (void)(expr); } while (0)
#define TEST_EXPECT_FALSE(expr)             do { TEST_PANIC(); (void)(expr); } while (0)
#define TEST_EXPECT_EQ(expected, actual)    do { TEST_PANIC(); (void)(expected); (void)(actual); } while (0)
#define TEST_EXPECT_STREQ(expected, actual) do { TEST_PANIC(); (void)(expected); (void)(actual); } while (0)
#define TEST_ASSERT_TRUE(expr)              do { TEST_PANIC(); (void)(expr); } while (0)
#define TEST_ASSERT_FALSE(expr)             do { TEST_PANIC(); (void)(expr); } while (0)
#define TEST_ASSERT_EQ(expected, actual)    do { TEST_PANIC(); (void)(expected); (void)(actual); } while (0)
#define TEST_ASSERT_STREQ(expected, actual) do { TEST_PANIC(); (void)(expected); (void)(actual); } while (0)

#endif /* OPAL_TEST */

#endif
