#ifndef OPAL_TEST_H
#define OPAL_TEST_H

#include <stdint.h>

#include <kc/attributes.h>
#include <kc/string.h>
#include <kc/assert.h>

#ifdef TEST
#define STATIC_OR_TEST
#else
#define STATIC_OR_TEST static
#endif

typedef void (*unit_test_ptr)(void);

#define PROTOTYPE_UNIT_TEST(name) \
    static void unit_test_func__##name (void)

#ifdef UNIT_TEST

struct unit_test_info {
    void (*fn)(void);
    const char *item;
};

#if !__has_attribute(section)
#error "kernel unit test requires __attribute__((section))"
#elif !__has_attribute(used)
#error "kernel unit test requires __attribute__((used))"
#endif

#define DEFINE_UNIT_TEST(name) \
    PROTOTYPE_UNIT_TEST(name); \
    static const struct unit_test_info unit_test_item__##name = { \
        .fn = unit_test_func__##name , \
        .item = #name , \
    }; \
    static const struct unit_test_info *unit_test_ptr__##name \
    UNUSED_ATTR __attribute__((used, section(".unittest"))) = &unit_test_item__##name ; \
    PROTOTYPE_UNIT_TEST(name)

void unit_test_run(void);
void unit_test_expect_true_failed(const char *expr, const char *file, unsigned line);
void unit_test_expect_eq_u64_failed(
    const char *expected_expr,
    const char *actual_expr,
    uint64_t expected,
    uint64_t actual,
    const char *file,
    unsigned line
);
void unit_test_expect_streq_failed(
    const char *expected_expr,
    const char *actual_expr,
    const char *expected,
    const char *actual,
    const char *file,
    unsigned line
);

#define TEST_EXPECT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            unit_test_expect_true_failed(#expr, __FILE__, __LINE__); \
        } \
    } while (0)

#define TEST_EXPECT_FALSE(expr) TEST_EXPECT_TRUE(!(expr))

#define TEST_EXPECT_EQ(expected, actual) \
    do { \
        const uint64_t _test_expected = (uint64_t)(expected); \
        const uint64_t _test_actual = (uint64_t)(actual); \
        if (_test_expected != _test_actual) { \
            unit_test_expect_eq_u64_failed(#expected, #actual, _test_expected, _test_actual, __FILE__, __LINE__); \
        } \
    } while (0)

#define TEST_EXPECT_STREQ(expected, actual) \
    do { \
        const char *const _test_expected = (expected); \
        const char *const _test_actual = (actual); \
        if ((_test_expected == 0 && _test_actual != 0) || \
            (_test_expected != 0 && _test_actual == 0) || \
            (_test_expected != 0 && _test_actual != 0 && strcmp(_test_expected, _test_actual) != 0)) { \
            unit_test_expect_streq_failed(#expected, #actual, _test_expected, _test_actual, __FILE__, __LINE__); \
        } \
    } while (0)

#define TEST_ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            unit_test_expect_true_failed(#expr, __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_FALSE(expr) TEST_ASSERT_TRUE(!(expr))

#define TEST_ASSERT_EQ(expected, actual) \
    do { \
        const uint64_t _test_expected = (uint64_t)(expected); \
        const uint64_t _test_actual = (uint64_t)(actual); \
        if (_test_expected != _test_actual) { \
            unit_test_expect_eq_u64_failed(#expected, #actual, _test_expected, _test_actual, __FILE__, __LINE__); \
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
            unit_test_expect_streq_failed(#expected, #actual, _test_expected, _test_actual, __FILE__, __LINE__); \
            return; \
        } \
    } while (0)

#else

#define DEFINE_UNIT_TEST(name) UNUSED_ATTR PROTOTYPE_UNIT_TEST(name)
static inline void unit_test_run(void) {}

#define TEST_PANIC()                        panic("This executable is not configured as unit test.")
#define TEST_EXPECT_TRUE(expr)              TEST_PANIC()
#define TEST_EXPECT_FALSE(expr)             TEST_PANIC()
#define TEST_EXPECT_EQ(expected, actual)    TEST_PANIC()
#define TEST_EXPECT_STREQ(expected, actual) TEST_PANIC()
#define TEST_ASSERT_TRUE(expr)              TEST_PANIC()
#define TEST_ASSERT_FALSE(expr)             TEST_PANIC()
#define TEST_ASSERT_EQ(expected, actual)    TEST_PANIC()
#define TEST_ASSERT_STREQ(expected, actual) TEST_PANIC()

#endif

#endif
