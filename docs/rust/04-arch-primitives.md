# 04 Arch Primitives

이 단계는 Rust 커널의 x86_64 저수준 asm primitive를 `opal-kernel/src/arch/x86_64/`에 모으는 것을 목표로 합니다.

## 목표
- CPU instruction wrapper는 `arch/`가 소유합니다.
- `platform/pc-x64`는 serial, QEMU debug-exit 같은 장치 의미만 유지합니다.
- 이후 IRQ, memory manager, task 포팅에서 사용할 최소 x86_64 primitive를 준비합니다.

## 설계 결정
- 4단계 범위는 x86_64 전용 primitive입니다.
- `arch/mod.rs`는 현재처럼 선택된 architecture 구현을 re-export합니다.
- public API는 `snake_case` 함수와 `CpuidInfo` 구조체로 제공합니다.
- unsafe가 필요한 명령은 `unsafe fn`으로 유지하고 짧은 `# Safety` 문서만 둡니다.
- GDT/IDT/TSS load, FPU/SIMD save/restore, context switch, syscall/interrupt dispatch는 이 단계에 넣지 않습니다.

## 작업 목록
- port I/O primitive를 추가합니다.
  - `in8`, `in16`, `in32`
  - `out8`, `out16`, `out32`
  - `insw`, `outsw`
- interrupt/halt primitive를 추가합니다.
  - `interrupts_enabled`
  - `disable_interrupts`
  - `enable_interrupts`
  - `wait_for_interrupt`
  - `enable_interrupts_and_wait`
  - `halt_loop`
- flags/control primitive를 추가합니다.
  - `read_rflags`, `write_rflags`
  - `read_cr0`, `read_cr2`, `read_cr3`, `read_cr4`
  - `write_cr0`, `write_cr3`, `write_cr4`
  - `flush_tlb_addr`
- CPU query/MSR primitive를 추가합니다.
  - `CpuidInfo`
  - `cpuid`
  - `read_msr`, `write_msr`
- `platform/pc-x64` 호출부를 새 arch API 이름에 맞춥니다.

## 검증 명령
```bash
make RUST=1 CONFIG=debug PLATFORM=pc-x64
make RUST=1 CONFIG=release PLATFORM=pc-x64
make RUST=1 unit-test CONFIG=debug PLATFORM=pc-x64 QEMU_DISPNONE=1
make RUST=1 unit-test CONFIG=release PLATFORM=pc-x64 QEMU_DISPNONE=1
```

## 완료 기준
- `arch/x86_64`가 4단계 primitive를 public API로 제공합니다.
- `platform/pc-x64`에는 inline asm이 직접 남지 않습니다.
