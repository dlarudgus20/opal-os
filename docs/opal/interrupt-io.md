# Interrupt / IO Path

## 부팅 시 초기화 순서 (`kmain`)
- 구현: `kernel/src/kmain.c`
- 현재 흐름:
  - `tty0_init() -> uart_early_init() -> klog_init()`
  - `boot_info_init() -> descriptors_init()`
  - `mm_init() -> fb_init() -> hid_init()`
  - `irq_init() -> timer_init() -> sched_init()`
  - `ps2_init() -> uart_init()`
  - `irq_enable_intr() -> interrupts_enable()`
  - `shell_start()`
  - `irqmsg_drain_loop()`

## IRQ 처리 모델
- top-half: 플랫폼 IRQ 핸들러가 최소 작업 후 `irqmsg_push()`로 메시지 큐잉
- bottom-half: `irqmsg_drain_loop()`에서 메시지를 pop해 실제 핸들러 실행
- 구현: `kernel/src/irq.c`
- 큐:
  - ringbuffer 크기 `IRQ_QUEUE_SIZE=4096`
  - overflow 시 `IRQMSG_DROP` 이벤트를 별도로 1회 이상 보고

## 타이머 경로
- 구현: `kernel/src/timer.c`
- PIT IRQ(IRQ0)에서:
  - `g_tick++`
  - `irq_send_eoi(PIC_IRQ_TIMER)`
  - `schedule()`
- `timer_get_tick()`은 atomic load로 읽기

## PS/2 입력 경로
- 구현: `kernel/platform/pc-x64/src/drivers/ps2/ps2.c`
- 초기화:
  - 컨트롤러 self-test, 포트 테스트, 디바이스 reset/enable
  - keyboard/mouse IRQ 등록 및 enable
- 인터럽트:
  - `ps2_drain_output()`가 컨트롤러 output buffer를 drain
  - keyboard 바이트는 `IRQMSG_PS2_KBD`
  - mouse 바이트는 `IRQMSG_PS2_MOUSE`
- bottom-half:
  - `ps2_keyboard_feed_raw()`, `ps2_mouse_feed_raw()` 호출

## HID 경로
- 구현: `kernel/src/hid/hid.c`
- 키 이벤트:
  - `hid_report_key()`가 pressed 상태 배열 갱신
  - `CapsLock`/`NumLock`/`ScrollLock`은 HID 계층에서 토글 상태로 관리
  - printable key는 `tty0` 입력 경로로 전달
- 포인터 이벤트:
  - `hid_report_pointer()`가 framebuffer 경계 내로 좌표 clamp
  - 커서 사각형 invert 방식으로 이동 표시

## UART 경로
- 구현: `kernel/platform/pc-x64/src/drivers/uart.c`
- early 단계:
  - `uart_early_init()`에서 포트 probe 및 기본 UART 선택
  - polling read/write 경로 사용
- irq 단계:
  - `uart_init()`에서 RX/TX ringbuffer 초기화
  - COM1/COM2 IRQ 등록 후 `irq_mode=true`
  - 기본 UART가 있으면 `uart_tty_enable_irqmsg()`도 여기서 함께 호출
  - `uart_try_write()/uart_try_read()`가 ringbuffer 중심으로 동작
- TTY 연계:
  - 구현: `kernel/src/tty/uart_tty.c`
  - `uart_tty_init()`이 기본 UART를 `tty0` sub-tty로 등록
  - `uart_tty_enable_irqmsg()`가 `IRQMSG_UART_RX` 핸들러를 등록
  - UART ISR은 RX 바이트를 소프트웨어 RX 큐에 적재한 뒤 `IRQMSG_UART_RX`를 큐잉
  - bottom-half에서 `uart_try_read()`로 바이트를 가져와 `tty0` 입력 경로로 넘긴다
  - `irqmsg_push()` 실패 시 RX 바이트는 UART RX 큐에 남고, 다음 RX IRQ가 와야 다시 소비 기회가 생긴다
