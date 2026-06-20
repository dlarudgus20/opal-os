# utest

`utest`는 사용자 공간 syscall 테스트 프로그램입니다.

## 동작
- `devfs`를 `/dev`에 마운트한다.
- `/dev/fbcon`을 stdout으로 열고 결과를 화면에 출력한다.
- 각 테스트는 `[ok]` 또는 `[fail]` 행을 출력하고, 실패 수를 누적한다.
- 마지막에 `utest: N failure(s)`를 출력한다.
- 종료 코드는 실패 수가 0이면 `0`, 하나 이상이면 `1`이다.

## 테스트 항목
- fbcon 출력
  - stdout에 문자열을 쓴다.
  - `FBCON_IOCTL_COLOR`로 색상 reset을 요청한다.
  - `FBCON_IOCTL_GET_CURSOR`로 커서 위치를 읽는다.
- FD 복제
  - `dup(FD_STDOUT, FD_INVALID)`로 빈 FD에 stdout을 복제한다.
  - 복제된 FD로 문자열을 쓴 뒤 닫는다.
- HID 입력 파일
  - `/dev/hid`를 stdin으로 연다.
  - 성공하면 stdin을 닫는다.
- 파일 읽기
  - `/utest`를 read-only로 연다.
  - 앞 16바이트를 읽고 FD를 닫는다.
- pipe
  - `pipe()`로 read/write end를 만든다.
  - write end에 `"pipe"` 문자열을 쓴다.
  - read end에서 같은 크기를 읽고 payload를 비교한다.
  - 양쪽 pipe FD를 닫는다.

## 주의사항
- 이미 `/dev`가 마운트된 상태에서 실행하면 `mount(devfs, /dev)`는 실패할 수 있으며, 현재 테스트는 그 결과를 그대로 failure로 집계한다.
- `utest`는 사람이 화면 출력을 확인하는 smoke test이며, 커널 hosted/unit test 프레임워크와 별개다.
