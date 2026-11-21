# 커널 테스트 프레임워크

## 1. 개요

`make test`는 `TEST=1`로 테스트용 커널과 ISO를 빌드하고 QEMU에서 실행합니다. 공통 빌드 설정은 이 구성에서 커널에 `OPAL_TEST`를 정의합니다. 테스트 실행기는 커널 소스에 있으며, 부팅 중에 테스트를 실행하고 COM1 시리얼 포트로 결과를 출력합니다. [테스트 API](../kernel/include/opal/test.h), [실행기](../kernel/src/test.c)

## 2. 테스트 정의

공통 테스트는 `kernel/tests/`에, 플랫폼 전용 테스트는 `kernel/platform/$(PLATFORM)/tests/`에 둡니다. 각 테스트는 `<opal/test.h>`를 포함하고 `DEFINE_TEST(name)`으로 등록합니다. 이름은 로그에 그대로 표시되므로 검증하는 동작을 드러내는 이름을 붙입니다.

```c
#include <opal/test.h>
#include <kc/stdio.h>

DEFINE_TEST(snprintf_unsigned) {
    char buffer[8];

    TEST_ASSERT_EQ(3, ksnprintf(buffer, sizeof(buffer), "id%u", 7u));
    TEST_EXPECT_STREQ("id7", buffer);
}
```

| 매크로 | 검증 내용 |
| --- | --- |
| `TEST_EXPECT_TRUE`, `TEST_ASSERT_TRUE` | 조건이 참인지 확인합니다. |
| `TEST_EXPECT_FALSE`, `TEST_ASSERT_FALSE` | 조건이 거짓인지 확인합니다. |
| `TEST_EXPECT_EQ`, `TEST_ASSERT_EQ` | 기대값과 실제값을 `uint64_t`로 변환해 비교합니다. |
| `TEST_EXPECT_STREQ`, `TEST_ASSERT_STREQ` | 문자열 내용을 비교합니다. 두 값이 모두 널 포인터인 경우에도 일치로 처리합니다. |

`TEST_EXPECT_*`는 실패를 기록하고 현재 테스트를 계속합니다. `TEST_ASSERT_*`는 실패를 기록하고 현재 테스트 함수에서 즉시 반환하므로, 후속 검증에 필요한 전제 조건을 검사할 때 사용합니다. 실행기는 같은 그룹의 다음 테스트를 이어서 실행합니다.

테스트는 초기 부팅 단계에서 실행되므로 필요한 상태를 각 테스트에서 직접 준비하고, 다른 테스트의 실행 순서에 의존하지 않도록 작성합니다. [기존 테스트 예시](../kernel/tests/utils.c)

## 3. 등록과 링크

1. `DEFINE_TEST(name)`은 테스트 함수와 이름을 담은 `struct test_info`를 만들고, 그 구조체를 가리키는 포인터를 입력 섹션 `.test`에 배치합니다. 포인터에 붙은 `gnu::used`는 컴파일러가 이 등록 정보를 오브젝트에 남기도록 합니다.
2. 플랫폼 링커 스크립트는 `KEEP(*(.test))`로 등록 포인터를 `.rodata`에 모읍니다. `KEEP`은 링크 시 미사용 섹션을 제거하는 경우에도 이 등록 정보를 유지합니다.
3. 링커가 정의하는 `test_start__`와 `test_end__`가 포인터 배열의 범위를 표시합니다. 실행기는 이 범위를 순회하며 등록된 테스트를 찾습니다.

새 테스트 파일은 테스트 디렉터리에 추가하면 빌드 규칙이 수집합니다. 호출 목록을 수동으로 관리할 필요가 없습니다. [공통 빌드 규칙](../mkfiles/rules.mk), [pc-x64 링커 스크립트](../kernel/platform/pc-x64/src/linker.ld)

## 4. 실행과 결과

1. `kmain()`이 `test_run()`을 호출합니다. 실행기는 첫 실행에서 COM1을 초기화한 뒤 `==== test run ====`을 출력합니다.
2. `test_run()`은 이름이 `heavy__`로 시작하지 않는 테스트를 실행합니다. `heavy__`로 시작하는 테스트를 실행하려면 `test_run_heavy()`를 명시적으로 호출합니다. 두 함수는 호출마다 자신의 실행 결과를 새로 집계합니다.
3. 각 테스트의 시작과 종료에 `[ RUN      ]`, `[       OK ]` 또는 `[  FAILED  ]`가 표시됩니다. 검증 실패에는 소스 파일, 줄 번호, 조건이나 비교식이 표시됩니다. 값 비교의 경우 기대값과 실제값도 출력됩니다.
4. 실행기가 `==== test end ====`와 함께 결과를 출력합니다. `run`, `pass`, `fail`은 테스트 함수의 개수입니다. 한 테스트에서 검증이 여러 번 실패해도 `fail`에는 테스트 한 건으로 집계됩니다. 실패한 테스트가 있으면 요약을 출력한 뒤 panic합니다.

테스트 함수에서 panic이 발생하면 `test_abort()`가 해당 테스트를 실패로 집계하고 중단 표시와 요약을 출력한 뒤 QEMU 실패 종료를 요청합니다. 테스트 그룹이 실행 중이 아닐 때 `test_abort()`는 즉시 반환합니다.

세 테스트가 모두 통과한 경우 마지막 줄은 다음과 같습니다.

```text
==== test end ==== run=3 pass=3 fail=0
```

## 5. 빌드와 실행

저장소 루트에서 테스트용 ISO를 먼저 빌드한 뒤 QEMU를 실행합니다.

```bash
make iso TEST=1 >/dev/null
make test QEMU_DISPNONE=1
```

`QEMU_DISPNONE=1`은 그래픽 창을 끄고 시리얼 출력을 터미널에 남깁니다. 그래픽 창을 사용하려면 `make test`를 실행합니다. 기본 설정에서는 테스트 결과를 출력한 뒤 QEMU가 자동으로 종료됩니다. COM1의 `==== test end ====` 요약과 `make test`의 종료 상태를 확인합니다.

테스트용 커널만 빌드하거나 릴리스 구성으로 검증할 수도 있습니다.

```bash
make -C kernel build TEST=1 >/dev/null
make iso TEST=1 CONFIG=release >/dev/null
make test CONFIG=release QEMU_DISPNONE=1
```

커널 산출물은 `kernel/build/test/<platform>/<config>/`, ISO는 `build/test/<platform>/<config>/`에 생성됩니다. 현재 구성의 테스트 산출물은 `make clean-test`로 정리합니다. [빌드 시스템](build-system.md)

## 6. 일반 빌드에서의 동작

- `test_run()`과 `test_run_heavy()`는 즉시 반환합니다. 호출하는 쪽은 `OPAL_TEST`를 검사할 필요가 없습니다.
- 빌드 규칙은 `TEST=1`일 때 테스트 디렉터리를 수집합니다. 공용 소스에서 `DEFINE_TEST()`를 사용하면 일반 빌드에서는 `[[maybe_unused]]` 정적 함수가 생성되고 등록 포인터는 만들어지지 않습니다.
- 일반 빌드에서 `TEST_EXPECT_*`나 `TEST_ASSERT_*`가 있는 코드 경로를 실제로 실행하면 panic합니다. 일반 커널의 조건 검증에는 `kassert()`를 사용합니다.

## 7. QEMU debug exit

pc-x64의 `<opal/platform/debug_exit.h>`는 플랫폼이 지원한다면 `PLATFORM_HAS_DEBUG_EXIT`를 정의합니다.

```c
#include <opal/platform/debug_exit.h>

void try_debug_exit(void) {
#ifdef PLATFORM_HAS_DEBUG_EXIT
    platform_debug_exit(true);
#endif
}
```

pc-x64에서 `make test`는 QEMU에 `isa-debug-exit` 장치를 추가합니다. `platform_debug_exit()`는 I/O 포트 `0x501`에 성공이면 `0x10`, 실패면 `0x11`을 씁니다. QEMU는 `(code << 1) | 1`을 적용해 각각 `33`과 `35`를 반환합니다. `run` 레시피는 이를 각각 성공 상태 `0`과 테스트 실패 상태 `1`로 변환합니다. 그 외 QEMU 상태는 레시피 오류 `2`로 처리합니다.

`QEMU_DEBUG_EXIT=0`을 지정하면 장치 추가와 종료 상태 변환을 끕니다. 이 경우 테스트 요약을 확인한 뒤 Ctrl+C로 QEMU를 종료합니다. `PLATFORM_HAS_DEBUG_EXIT`는 빌드 시점의 지원 여부를 나타내며, 장치가 없는 QEMU에서는 `platform_debug_exit()`가 반환합니다.
