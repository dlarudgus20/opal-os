# Device Files

## 개요
커널은 `devfs`를 통해 장치 파일을 노출한다. 이 문서는 `devfs`가 `/dev`에 마운트되어 있다는 전제로 장치 파일 경로를 설명한다.

## `/dev/fbcon`
- 구현: `kernel/src/fb/fbcon.c`
- open mode:
  - `OPEN_NONE`
  - `OPEN_WRITE | OPEN_APPEND`
- `read`, `seek`, `truncate`는 지원하지 않는다.
- `write`는 입력 바이트를 현재 커서 위치에 글리프로 그린다.
- `write`는 `'\n'`, `'\b'` 같은 제어문자를 해석하지 않는다. 줄바꿈, backspace, echo, line editing은 사용자 공간 TTY discipline의 책임이다.
- 마지막 컬럼에서 더 쓰면 커서는 더 이동하지 않는다. 스크롤은 ioctl로 명시적으로 요청해야 한다.

### fbcon ioctl
상수는 커널 공용 헤더가 아니라 `fbcon.c`와 사용자 프로그램 쪽에 로컬로 유지한다.

- `0`: color
  - `arg == 0xffff`: foreground/background 기본값으로 reset
  - 하위 8비트: foreground 색. `0xff`면 변경 없음
  - 상위 8비트: background 색. `0xff`면 변경 없음
  - 색 번호는 0..15 팔레트 인덱스
- `1`: get cursor
  - 반환값 하위 16비트: x
  - 반환값 상위 16비트: y
- `2`: gotoxy
  - `arg = x | (y << 16)`
  - 화면 밖 좌표는 `OPAL_ERANGE`
- `3`: set cursor visible
  - `arg == 0`: 숨김
  - `arg == 1`: 표시
- `4`: put at
  - `arg = x | (y << 8) | (ch << 16)`
  - 현재 커서 위치는 바꾸지 않고 지정 좌표에 글리프를 그린다
- `5`: erase line range
  - `arg = x0 | (y << 8) | (x1 << 16)`
  - `[x0, x1)` 범위를 공백 문자로 덮는다
- `6`: scroll up
  - `arg`는 0이어야 한다
  - 화면 내용을 한 줄 위로 올리고 마지막 줄을 지운다
