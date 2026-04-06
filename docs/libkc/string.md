# libkc 문자열/메모리 API

## 표준 계열
- 메모리: `memcpy`, `memmove`, `memset`, `memcmp`, `memchr`
- 문자열 탐색/비교: `strlen`, `strspn`, `strcspn`, `strchr`, `strrchr`, `strcmp`, `strncmp`
- 문자열 연결/복사: `strcpy`, `strcat`, `strncat`, `strcpy_sized`

## Annex K 계열
- `strnlen_s`: 최대 길이를 넘지 않게 문자열 길이를 계산합니다.

## 비표준/확장 계열
- `memchr_not`: 지정한 바이트와 다른 첫 위치를 찾습니다.
- `strnchr`: 길이 제한(`n`) 내에서 문자를 찾습니다.
- `strcpy_sized`: 대상 버퍼 크기를 기준으로 안전하게 전체 복사합니다.
- `strncpy_sized`: 대상 버퍼 크기를 기준으로 복사 길이를 제한합니다.
- `strcat_sized`: 대상 버퍼 크기를 기준으로 이어붙입니다.

### `strcpy_sized` 동작
- 시그니처: `void strcpy_sized(char *dest, size_t destsz, const char *src)`
- 동작 요약:
  - `destsz == 0`이면 아무 작업 없이 반환합니다.
  - 최대 `destsz - 1` 바이트까지만 복사합니다.
  - 복사 후 항상 NUL 종료를 보장합니다.

### `strncpy_sized` 동작
- 시그니처: `void strncpy_sized(char *dest, size_t destsz, const char *src, size_t n)`
- 동작 요약:
  - `destsz == 0`이면 아무 작업 없이 반환합니다.
  - 실제 복사 길이는 `min(n, destsz - 1)`로 제한됩니다.
  - 복사 후 항상 `dest`에 NUL 종료 문자를 기록합니다.
- 호출자 계약:
  - `dest`/`src`는 유효한 포인터여야 합니다.
  - 소스/대상 버퍼 겹침(overlap)은 허용하지 않습니다.

### `strcat_sized` 동작
- 시그니처: `void strcat_sized(char *dest, size_t destsz, const char *src)`
- 동작 요약:
  - `dest` 내부에 `destsz` 범위의 NUL이 없으면 변경 없이 반환합니다.
  - 남은 공간 내에서 최대한 이어붙이고 NUL 종료를 유지합니다.
