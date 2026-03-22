# libkc 기타 공개 헤더

이 문서는 별도 문서가 없는 공개 헤더를 정리합니다.

## `stdlib.h`
- 정렬/유틸리티:
  - `align_ceil_u32_p2`, `align_ceil_sz_p2`, `align_floor_sz_p2`
  - `ispower2`
- 문자열 숫자 파싱:
  - `kstrtoul(const char *str, int base, char **endptr, unsigned long *result)`
  - `kstrtoul_exact(const char *str, int base, unsigned long max, unsigned long *result)`
- 정렬 함수:
  - `sort(void *ptr, size_t count, size_t size, int (*comp)(const void *, const void *))`
  - 현재 구현은 힙 정렬 기반입니다.
- 매크로:
  - `MAX(a, b)`, `MIN(a, b)`
  - `container_of(ptr, type, member)`

## `span.h`
- 구조체:
  - `struct span { void *ptr; size_t size; }`
- 목적:
  - 포인터 + 길이 페어를 전달하는 단순 뷰 타입입니다.

## `inttypes.h`
- 목적:
  - `printf` 계열 포맷 문자열 매크로(`PRI*`) 제공
- 제공 매크로:
  - `PRId*`, `PRIi*`, `PRIu*`, `PRIx*`, `PRIX*`
  - 포인터용 `PRIdPTR`, `PRIiPTR`, `PRIuPTR`, `PRIxPTR`, `PRIXPTR`
- 구현 메모:
  - 현재 prefix는 `PRI64_PREFIX`, `PRIPTR_PREFIX` 매크로로 정의합니다.

## `attributes.h`
- 목적:
  - 컴파일러 attribute 추상화 제공
- `__has_attribute`:
  - 컴파일러가 `__has_attribute`를 제공하지 않으면 `#define __has_attribute(x) 0`로 대체합니다.
  - 이 덕분에 지원 여부 분기를 모든 컴파일러에서 동일 문법으로 작성할 수 있습니다.
- `PRINTF_ATTR(a, b)`:
  - 컴파일러가 `format` attribute를 지원하면 `[[gnu::format(printf, a, b)]]`를 사용합니다.
  - 미지원 환경에서는 빈 매크로로 처리합니다.

## `ctype.h`
- 목적:
  - 최소 문자 분류 유틸리티 제공
- 현재 제공 함수:
  - `static inline bool isdigit(char ch)`
  - `static inline bool isspace(char ch)`

## `errno.h`
- 목적:
  - 표준 C `errno` 전역 변수 대신, 함수 반환형으로 사용할 최소 에러 코드 집합 제공
- 타입:
  - `typedef enum errno errno_t;`
- 현재 에러 코드:
  - `E_OK = 0`: 성공
  - `EINVAL`: 인자/형식 오류
  - `ERANGE`: 값 범위 초과

## 파싱 함수 규약 (`kstrtoul*`)
- `kstrtoul`:
  - 선행 공백을 건너뛴다.
  - `+` 부호는 허용, `-` 부호는 `EINVAL`.
  - `base=0`이면 접두사/선행 0에 따라 진법을 자동 선택한다.
  - 유효 숫자가 하나도 없으면 `EINVAL`.
  - overflow 시 `ERANGE`.
  - 성공 시 `E_OK`.
- `kstrtoul_exact`:
  - 내부적으로 `kstrtoul`을 호출한다.
  - 파싱 이후 trailing 문자가 있으면 `EINVAL`.
  - 파싱 결과가 `max`보다 크면 `ERANGE`.
  - 정상 완료 시 `E_OK`.
