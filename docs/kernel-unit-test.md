# Kernel Unit Test Framework

## 1. 개요
커널 유닛 테스트는 `make unit-test`로 실행되며 컴파일 시 `OPAL_UNIT_TEST` 매크로가 정의됩니다.
핵심 API는 [`kernel/include/opal/test.h`](../kernel/include/opal/test.h)에 정의되어 있습니다.

## 2. 테스트 정의
```c
DEFINE_UNIT_TEST(name) {
    TEST_EXPECT_EQ(1, 1);
    TEST_EXPECT_TRUE(cond);
}
```

테스트 매크로:
- `TEST_EXPECT_TRUE/FALSE`
- `TEST_EXPECT_EQ/STREQ`
- `TEST_ASSERT_TRUE/FALSE`
- `TEST_ASSERT_EQ/STREQ`

`TEST_ASSERT_*`는 실패 시 현재 테스트를 즉시 종료합니다.

## 3. 작동 메커니즘
`DEFINE_UNIT_TEST`는 테스트 포인터를 `.unittest` 섹션에 배치합니다.
- `__attribute__((used, section(".unittest")))`
- linker script에서 `.rodata` 섹션에 배치합니다.

## 4. 실행 흐름
1. `kmain()`에서 `unit_test_run()` 호출
2. `.unittest` 섹션을 순회하며 테스트 실행
3. 실패 집계
4. 실패가 있으면 panic

## 5. 빌드/실행
```bash
make -C kernel build CONFIG=debug PLATFORM=pc-x64 UNIT_TEST=1
make unit-test CONFIG=debug PLATFORM=pc-x64
```

## 6. 비-유닛테스트 빌드
- `unit_test_run()`은 no-op
- `TEST_EXPECT_*`, `TEST_ASSERT_*`는 `panic`로 대체
