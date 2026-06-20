# Kernel TTY / Console Path

## 개요
- 구현:
  - `kernel/src/tty/tty.c`
  - `kernel/src/tty/uart_tty.c`
- `tty0`는 전역 콘솔 fan-out 지점이다
- 커널 로그와 커널 shell 출력은 등록된 sub-tty들로 전파된다
- 커널 shell 입력은 `tty0` 내부 ringbuffer에 모인다

## 출력 경로
- `tty0_puts()`/`tty0_printf()`는 `tty0`에 연결된 sub-tty로 fan-out 된다
- 현재 주된 sub-tty:
  - 기본 UART TTY
- buffered TTY는 내부 버퍼를 사용하고, 기본적으로 `'\n'`이 포함된 출력에서 flush된다
- UART TTY는 상위 `tty_buffered`와 UART 내부 TX 큐를 함께 사용하므로 flush 시점이 가시성에 직접 영향을 준다
- `tty0_getchar()`는 입력 대기 직전에 `tty0_flush()`를 호출해 지연된 prompt가 먼저 보이게 한다
- 따라서 prompt처럼 개행 없는 출력은 입력 대기 전에 flush가 필요하다

## 입력 경로
- `tty0`는 출력 fan-out과 별개로 전역 입력 ringbuffer를 가진다
- 저장 구조:
  - `inbuf`, `in_head`, `in_tail`, `inlen`
  - `input_ev`는 "입력 버퍼가 비어 있지 않음" 상태를 나타낸다
- `tty0_put_input()`:
  - 입력 바이트를 ringbuffer에 append
  - 버퍼 full 상태에서는 일반 문자를 drop
  - 버퍼 full 상태의 `'\n'`은 마지막 저장 칸을 `'\n'`으로 덮어써 줄 종료를 보존
  - 입력 버퍼가 비어 있다가 처음 채워질 때만 `input_ev`를 signal
- `tty0_getchar()`:
  - 버퍼가 비어 있으면 `event_wait()`로 block
  - 깨어난 뒤 head에서 1바이트를 pop
  - 버퍼가 다시 비면 `input_ev`를 reset
- `tty0_getline()`:
  - `tty0_getchar()`를 반복 호출해 `'\n'`까지 한 줄 전체를 소비
  - 반환 문자열에는 `'\n'`을 포함하지 않는다
  - 버퍼보다 긴 줄은 잘라서 반환하고, 같은 줄의 나머지는 버린다
- 따라서 긴 줄이 들어오면 앞부분만 반환되고, 초과 입력은 복구되지 않는다

## 입력 소스별 정책
- 시리얼:
  - UART ISR이 RX 바이트를 소프트웨어 RX 큐에 적재
  - `IRQMSG_UART_RX` bottom-half가 `uart_try_read()` 후 `tty0_put_input()` 호출
  - `'\r'`은 `'\n'`으로 정규화한다
  - 시리얼 로컬 echo는 `write_char_tty()`를 통해 `'\n' -> "\r\n"` 변환 후 해당 UART에 다시 기록한다
  - echo 경로는 `uart_suppress_tx()`로 TX IRQ 펌프를 잠시 억제해, `"\r\n"`이 쪼개져 순서가 뒤틀리는 경우를 줄인다

## panic 출력 모드
- `_panic_format`은 실제 panic 문자열 출력 직전에 `tty->ops->set_panic_mode`를 호출한다
- UART TTY의 `set_panic_mode`는 `uart_enter_panic_mode()`를 호출해 UART를 polling 쓰기 경로로 전환한다
- 이 경로는 panic 상황에서 IRQ 의존 없이 시리얼 출력이 진행되도록 하기 위한 것이다

## 현재 제약
- line editing은 아직 없다. `tty0_getline()`은 `'\n'`만 줄 종료로 해석한다
- 입력 overflow는 drop 정책이며, 손실 여부를 별도 에러로 보고하지 않는다
