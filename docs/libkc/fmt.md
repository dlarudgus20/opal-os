# libkc 포맷/stdio API

## 개요
- `fmt.h`:
  - `struct fmt`
  - `int fmt_sprintf(struct fmt *fmt, const char *format, ...)`
  - `int fmt_vsprintf(struct fmt *fmt, const char *format, va_list ap)`
- `stdio.h`:
  - `int snprintf_s(char *buffer, size_t bufsz, const char *format, ...)`
  - `int vsnprintf_s(char *buffer, size_t bufsz, const char *format, va_list ap)`

## `fmt.h`

### `struct fmt`
`fmt.h`의 핵심 상태 구조체이며 3가지 모드를 가집니다.

- 버퍼 모드:
  - `size > 0`
  - `buffer`에 출력하고 가능하면 NUL 종료를 유지합니다.
- write callback 모드:
  - `size == 0 && write_fn != NULL`
  - 문자마다 `write_fn(fmt, ch)`를 호출합니다.
- counting 모드:
  - `size == 0 && write_fn == NULL`
  - 실제 출력 없이 길이(`count`)만 누적합니다.

공통 필드:
- `count`: 작성 시도한 총 문자 수
- `error`: 내부 오류 상태

### `fmt_write`
- write callback 모드일 때 실제 fmt 출력을 수행하는 콜백 함수입니다.
- 타입: `typedef bool (*fmt_write)(struct fmt *fmt, char ch);`
- 반환이 `false`면 `fmt->error`가 설정되고 포맷 함수는 실패(`-1`)합니다.

### `fmt_sprintf`/`fmt_vsprintf`
- `printf` 스타일 포맷 엔진 진입점입니다.
- 정상 시 전체 문자 수(`count`)를 반환합니다.
- 실패 시 `-1`을 반환합니다.

## `stdio.h`

### `snprintf_s`/`vsnprintf_s`
- `fmt_vsprintf`를 버퍼 모드로 감싼 래퍼 API입니다.
- 입력 버퍼 크기(`bufsz`)를 검사하고, 오류 시 `-1`을 반환합니다.
- 포맷 실패 시 `buffer[0] = '\0'`로 정리합니다.

## 입력 검증과 반환 규칙
- `snprintf_s`/`vsnprintf_s`:
  - `buffer == NULL`: `-1`
  - `bufsz == 0` 또는 `bufsz > INT_MAX`: `-1`
  - `format == NULL`: `buffer[0] = '\0'` 후 `-1`
  - `fmt_vsprintf`가 음수 반환: `buffer[0] = '\0'` 후 음수 반환
- `fmt_vsprintf`:
  - `fmt == NULL`: `-1`
  - `fmt->size > INT_MAX`: `-1`
  - `format == NULL`: 버퍼 모드면 `*fmt->buffer = '\0'` 후 `-1`
- 정상 종료 시 반환값은 `count`(전체 문자 수, NUL 제외)입니다.

## 버퍼 초과 시 동작
- 버퍼 모드(`size > 0`)에서 출력 길이가 `size - 1`을 넘기기 시작하면:
  - 현재 위치에 즉시 `'\0'`를 기록합니다.
  - 이후 모드를 counting 모드(`size = 0`, `write_fn = NULL`)로 전환합니다.
  - 따라서 실제 버퍼 내용은 잘린 문자열로 NUL 종료되며, 추가 문자는 버퍼에 쓰지 않습니다.
- 반환값(`count`)은 잘린 결과 길이가 아니라 "원래 작성하려던 전체 길이"를 계속 누적한 값입니다.

## 포맷 지원 범위
- 지원되는 포맷
  - 정수 계열: `%d`, `%i`, `%u`, `%o`, `%x`, `%X`, `%p`
  - 문자/문자열: `%c`, `%s`, `%%`
  - 플래그/폭/정밀도: `-`, `+`, ` `, `#`, `0`, width(`숫자`/`*`), precision(`.` + 숫자/`*`)
  - 길이 수정자: `hh`, `h`, `l`, `ll`, `j`, `z`, `t`, `L`
- 미지원 포맷
  - `%n`: 실패 처리(`-1`)
  - 부동소수점(`%f`, `%e`, `%g` 등): 변환을 무시(출력 없음)
  - 알 수 없는 specifier/불완전 변환: 무시하고 계속 진행
