# uinit

`uinit`은 사용자 공간 init 프로그램입니다.

## 동작
1. `devfs`를 `/dev`에 마운트한다.
2. `/dev/hid`를 stdin으로 연다.
3. `/dev/fbcon`을 stdout으로 열고 stderr를 stdout에 복제한다.
4. pipe 두 개를 만든다.
5. `fork()`로 TTY reader, TTY writer, shell process를 구성한다.
6. shell process는 pipe end를 fd 0/1/2에 배치한 뒤 `/opsh`를 `exec(fd)`로 실행한다.

## TTY bridge
- shell stdout/stderr pipe에서 읽은 byte stream을 `/dev/fbcon` ioctl/write로 화면에 반영한다.
- `/dev/hid`에서 `struct hid_char` packet을 읽어 line buffer를 만든 뒤 shell stdin pipe에 쓴다.
- backspace는 line buffer에서 한 글자를 제거하고 화면 커서를 되돌린다.
- Enter는 화면 줄바꿈을 수행하고, line buffer와 `'\n'`을 shell stdin에 전달한다.
- printable 문자는 line buffer에 추가하고 화면에 echo한다.

## 화면 제어
- `uinit`은 fbcon ioctl을 직접 사용해 cursor 조회/이동, 지정 좌표 출력, 스크롤을 수행한다.
- ANSI SGR color sequence 중 일부를 해석한다.
  - `0`: reset
  - `30..37`, `90..97`: foreground
  - `40..47`, `100..107`: background
- `\n`과 backspace를 처리한다.

## 제한
- line buffer 길이는 256바이트다.
- overflow 시 추가 printable 입력은 버린다.
- 현재 escape parser는 color sequence 위주이며 일반 cursor escape sequence는 처리하지 않는다.
