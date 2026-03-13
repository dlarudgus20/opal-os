# Boot Sequence

## `kmain` 초기화 순서
- 구현: `kernel/src/kmain.c`
- 현재 흐름:
  - `tty0_init() -> uart_early_init() -> klog_init()`
  - `boot_init() -> kargs_init()`
    - `bootinfo_init() -> descriptors_init()`
  - `mm_init() -> fb_init() -> hid_init()`
  - `irq_init() -> timer_init() -> sched_init()`
  - `drivers_init()`
    - `ps2_init() -> uart_init()`
  - `irq_enable_intr() -> interrupts_enable()`
  - `run_user()`
    - 유닛테스트로 빌드되었다면 `unit_test_run()`
    - `shell_start()`
  - `irqmsg_drain_loop()`

## 부트 인자(`kargs`) 처리
- 구현: `kernel/src/kargs.c`
- 입력: `bootinfo_get_cmdline()`
- 형식: `opt=val` (공백 구분)
- 현재 유효 옵션:
  - `initramfs=<module_name>`
    - `bootinfo_get_modules()`에서 동일 이름 모듈을 찾아 `kargs.initramfs` 설정
- quoted value 지원:
  - `\"`, `\\` 이스케이프
- 파싱 중 잘못된 토큰/미지원 옵션/모듈 미존재는 경고 로그 후 계속 진행
