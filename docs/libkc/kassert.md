# libkc `kassert.h`

## 개요
`kassert.h`는 커널 런타임에서 조건 위반을 즉시 중단시키기 위한 매크로 집합입니다.

모든 실패 경로는 `_panic_format()`으로 모이며, 호출 위치(`file`, `func`, `line`)를 함께 전달합니다.
libkc를 사용하는 측에서 `_panic_format()` 구현을 제공해야 합니다.

## 공개 인터페이스

### `panic(msg)`
- 고정 문자열 메시지로 panic을 발생시킵니다.
- 메시지가 비어 있으면 `"panic"`만 출력합니다.

### `panicf(fmt, ...)`
- `printf` 스타일 메시지로 panic을 발생시킵니다.

### `kassert(...)`
- 1개 인자: `kassert(exp)`
  - 실패 시 `"assertion failed : exp"` 메시지로 panic
- 2개 인자: `kassert(exp, msg)`
  - 실패 시 `"msg : exp"` 메시지로 panic

### `kassertf(exp, fmt, ...)`
- 실패 시 `printf` 스타일 사용자 메시지 + 표현식 문자열(`exp`)을 함께 출력합니다.

## 외부 제공 심볼

### `_panic_format()`
- 선언
  - `[[noreturn]] void _panic_format(const char *fmt, const char *file, const char *func, unsigned line, ...);`
- panic 시 호출되며 메시지 출력 및 abort를 책임져야 합니다.
- `PRINTF_ATTR(1, 5)`가 적용되어 포맷 문자열/인자 타입 검증 대상입니다.
- `[[noreturn]]`이므로 호출 후 제어가 반환되지 않습니다.
