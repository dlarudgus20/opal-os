# kerrno

## 개요
- 헤더: `libkc/include/kc/kerrno.h`
- 목적:
  - 커널/라이브러리 공통 에러 코드를 `kerrno_t`로 통일
  - 문자열 변환(`kerrno_str`) 제공

## 타입
- `typedef enum kerrno kerrno_t;`

## 코드 규약
- `OPAL_OK = 0`:
  - 성공
- `OPAL_E* < 0`:
  - 실패
- 호출자는 `OPAL_OK` 여부로 성공/실패를 판정하고, 실패 시 코드를 그대로 상위로 전파하는 패턴을 사용합니다.

## 문자열 변환
- API: `const char *kerrno_str(kerrno_t err);`
- 구현: `libkc/src/kerrno.c`
- 용도:
  - `tty0_printf(..., kerrno_str(err), err)` 형태의 로그/진단 출력

## 사용 예시
```c
kerrno_t r = some_op(...);
if (r != OPAL_OK) {
    tty0_printf("op failed: %s (%d)\n", kerrno_str(r), r);
}
```
