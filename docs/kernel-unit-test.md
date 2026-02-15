# Kernel Unit Test Framework

## 1. 개요
커널 내부 유닛 테스트는 `UNIT_TEST=1` 빌드에서 활성화됩니다.
핵심 API는 [`kernel/include/opal/test.h`](../kernel/include/opal/test.h)에 정의되어 있습니다.

## 2. 테스트 정의
```c
DEFINE_UNIT_TEST(name) {
    TEST_EXPECT_EQ(1, 1);
    TEST_EXPECT_TRUE(cond);
}
```

매크로:
- `TEST_EXPECT_TRUE/FALSE`
- `TEST_EXPECT_EQ/STREQ`
- `TEST_ASSERT_TRUE/FALSE`
- `TEST_ASSERT_EQ/STREQ`

`ASSERT_*`는 실패 시 해당 테스트 함수를 즉시 종료합니다.

## 3. 등록 메커니즘
`DEFINE_UNIT_TEST`는 테스트 메타데이터 포인터를 `.unittest` 섹션에 배치합니다.
- `__attribute__((used, section(".unittest")))`
- linker script에서 `KEEP(*(.unittest))`로 보존

이 방식은 release + LTO에서 dead stripping되는 문제를 줄이기 위해 `used`가 필요합니다.

## 4. 실행 흐름
1. `kmain()`에서 `unit_test_run()` 호출
2. `unit_test_start__` ~ `unit_test_end__` 구간 순회
3. 각 테스트 실행 후 pass/fail 집계
4. 실패가 있으면 panic

## 5. 빌드/실행
```bash
make -C kernel build CONFIG=debug PLATFORM=pc-x64 UNIT_TEST=1
make unit-test CONFIG=debug PLATFORM=pc-x64
```

## 6. 비-UNIT_TEST 빌드에서의 동작
- `unit_test_run()`은 inline no-op
- `TEST_EXPECT_*`, `TEST_ASSERT_*`는 `TEST_PANIC()` 매크로로 정의됨
- LTO/GC 조건에 따라 실제 미사용 테스트 함수 코드는 제거될 수 있음
