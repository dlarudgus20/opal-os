# Kernel Log

## 개요
- 헤더: `kernel/include/opal/klog.h`
- 구현: `kernel/src/klog.c`
- 목적:
  - 커널 내부 로그를 순환 버퍼에 저장
  - TTY 출력과 shell의 `kmsg` 조회 경로에 로그 제공

## 공개 API
- `klog_init()`
  - 커널 로그 큐를 사용할 준비를 한다.
- `klog_write(level, msg, msglen)`
  - `msglen` 바이트 로그를 기록한다.
  - `level >= KLOG_LEVEL_COUNT`이면 마지막 로그 레벨로 clamp된다.
- `klog_read(header_out, msg_out, msg_size)`
  - 다음 로그 레코드를 읽는다.
  - 반환값:
    - `true`: 로그를 읽었고 출력값이 유효하다.
    - `false`: 읽을 로그가 없으며 출력값은 유효하지 않다.
  - `header_out` 출력값:
    - 성공 시 로그 sequence, message length, level을 반환한다.
  - `msg_out` 출력값:
    - 성공 시 NUL 종료된 메시지를 반환한다.
    - `msg_size`보다 긴 메시지는 `msg_size - 1` 바이트까지 복사된다.
  - `header_out != NULL`, `msg_out != NULL`, `msg_size > 0`은 assert 전제이다.
- `klog_print_all_tty0(seq)`
  - 현재 읽을 수 있는 로그를 모두 `tty0`에 출력한다.
- `klog_format(level, fmt, ...)`
  - printf 형식으로 로그 메시지를 만든 뒤 기록한다.

## 호출자 계약
- `klog_read()`의 `false` 반환은 정상적인 empty 상태이다.
- `klog_read()` 성공 후에도 호출자는 `msg_out` 버퍼 크기에 따른 truncation 가능성을 고려해야 한다.
