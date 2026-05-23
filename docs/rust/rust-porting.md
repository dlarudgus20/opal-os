# Rust Porting Plan

이 문서는 `opal-os` 커널을 Rust로 포팅하기 위한 설계 결정과 단계별 작업 계획을 기록합니다.

## 1. 기본 방향
- Rust 커널은 `RUST=1` 경로에서 빌드합니다.
- `make RUST=1`은 Rust 커널을 빌드하고, ISO 생성 시 Rust 버전 `kernel.sys`를 사용합니다.
- 새 Rust 커널은 Rust와 ASM만 사용합니다.
- 초기 Rust 지원 플랫폼은 `pc-x64`, 초기 CPU architecture는 `x86_64`만 대상으로 합니다.

## 2. 서브프로젝트 구조
- `opal-kernel/`
  - 커널 본체 Rust crate입니다.
  - `#![no_std]` 커널 코드, `kmain`, panic handler, kernel test harness, 부트 ASM을 소유합니다.
  - `rustc --crate-type rlib --emit=obj`로 `opal-kernel.rlib.o`를 만들고, ASM object와 `$(TOOLSET_LD) -r`로 합쳐 `opal-kernel.o`를 만듭니다.
  - 필요한 자료구조는 단계적으로 `opal-kernel::utils`에 둡니다.
- `opalkrnl/`
  - 최종 executable ELF를 만드는 링크용 서브프로젝트입니다.
  - Rust crate가 아니며 커널 코드를 넣지 않습니다.
  - `opal-kernel.o`, Rust sysroot rlib, linker script를 `$(TOOLSET_LD)`로 링크합니다.
- `opal-ktest/`
  - `#[ktest]` proc macro crate입니다.
  - 커널 본체가 직접 `rustc`로 빌드되므로, proc macro dylib만 Cargo로 host 빌드합니다.
- `rust-sysroot/`
  - `core`와 `compiler_builtins`를 custom target용 sysroot 형태로 준비합니다.
  - 이 경로는 Cargo `build-std`를 사용하지만, 커널 본체 빌드에는 Cargo를 사용하지 않습니다.

`kmain`은 반드시 `opal-kernel` 쪽에 둡니다.
부트 ASM이 호출하는 ABI 심볼도 `opal-kernel`이 export합니다.

## 3. 빌드 시스템
- Rust toolchain은 `mkfiles/config-rust.mk`의 `RUST_TOOLCHAIN` 기본값 `nightly-2026-04-14`로 고정합니다.
- Rust target spec은 `arch/$(ARCH)/$(RUST_TARGET_NAME).json`에 둡니다.
- Rust 공통 규칙은 `mkfiles/rules-rust.mk`에 둡니다.
- `rules-rust.mk`는 커널 본체에 Cargo를 사용하지 않습니다.
  - `static-obj`: Rust crate root를 `rustc`로 object화하고 ASM object와 `$(TOOLSET_LD) -r`로 합칩니다.
  - `executable`: object와 Rust sysroot rlib를 linker script로 최종 링크합니다.
- 루트 `Makefile`은 `RUST=1`일 때 `opalkrnl/`을 커널 빌드 대상으로 선택합니다.
- `RUST=1`일 때 `KERNEL_ELF`, `KERNEL_BIN`은 `opalkrnl` 산출물을 가리킵니다.
- `make RUST=1 iso`, `make RUST=1 run`, `make RUST=1 unit-test`를 Rust 경로의 사용자 인터페이스로 사용합니다.
- `UNIT_TEST=1` Rust 빌드는 `build/unit-test/<platform>/<config>`를 사용해 일반 `build/<platform>/<config>` 산출물과 겹치지 않습니다.

### 3.1 소스 경로 정책
- 공통 Rust/ASM 소스는 `src/` 아래에 둡니다.
- CPU architecture 전용 소스는 `src/arch/$(ARCH)/` 아래에 둡니다.
- platform 전용 소스는 `src/platform/$(PLATFORM)/` 아래에 둡니다.
- include 경로는 `include/`, `include/arch/$(ARCH)/`, `include/platform/$(PLATFORM)/`를 사용합니다.
- 현재 include 경로는 NASM 입력용으로 유지합니다.

### 3.2 부트 ASM과 링크
- 부트 ASM 파일과 조립 책임은 `opal-kernel`이 소유합니다.
- `opal-kernel/src/arch/$(ARCH)/**/*.asm`과 `opal-kernel/src/platform/$(PLATFORM)/**/*.asm`만 target-specific ASM으로 조립합니다.
- `opalkrnl` 링크 단계는 `opal-kernel.o`와 `opalkrnl`의 linker script를 사용합니다.
- linker script는 기존 higher-half 배치, `.startup`, `.text`, `.rodata`, `.data`, `.bss` 섹션 의미를 보존합니다.
- Rust 테스트 등록 영역은 `.ktest`를 사용합니다.

## 4. Rust 런타임 정책
- 1단계 런타임은 `core`만 사용합니다.
- `#![no_std]`, panic handler, halt loop를 먼저 구성합니다.
- allocator와 `alloc` crate는 memory manager 포팅 이후 도입합니다.
- 초기 출력은 serial smoke output 수준으로 시작합니다.
- 포맷팅, 문자열, 자료구조는 `opal-kernel` 내부 Rust 구현으로 단계적으로 마련합니다.

## 5. 테스트 정책
- Rust 서브프로젝트의 `unit-test`는 Rust `#[ktest]` 기반 커널 테스트로 취급합니다.
- 테스트는 QEMU에서 실행되는 kernel test image를 목표로 합니다.
- 테스트 이미지는 `UNIT_TEST=1`과 `--cfg opal_ktest`로 빌드합니다.
- `make RUST=1 unit-test QEMU_DISPNONE=1`이 공식 실행 경로입니다.

## 6. 포팅 단계
각 단계는 구현 전에 `docs/rust/nn-some-step-name.md` 형식의 상세 문서를 먼저 작성합니다.
- `nn`은 두 자리 단계 번호입니다.
- `some-step-name`은 소문자 kebab-case 영문 이름입니다.
- 상세 문서에는 해당 단계의 목표, 설계 결정, 작업 목록, 검증 명령, 완료 기준을 적습니다.
- 단계 구현 중 새로 확정된 결정은 해당 단계 문서와 이 포팅 계획에 함께 반영합니다.
- 포팅 중 기존 문서를 갱신해야 할 때는 원본 문서를 직접 바꾸지 않고, 수정본을 `docs/rust/docs/` 아래에 복사해 둡니다.

### 6.1 빌드 골격과 smoke boot
- 상세 문서: `docs/rust/01-build-smoke-boot.md`
- `opal-kernel` 커널 본체와 `opalkrnl` 링크 서브프로젝트를 추가합니다.
- `mkfiles/rules-rust.mk`를 추가합니다.
- 루트 `Makefile`에 `RUST=1` 분기를 추가합니다.
- `opal-kernel`이 `kmain`과 panic handler를 제공합니다.
- `opal-kernel`이 부트 ASM을 소유하고 `opal-kernel.o`에 함께 합칩니다.
- `make RUST=1 run QEMU_DISPNONE=1`로 Rust `kmain` 진입과 serial 출력까지 확인합니다.

### 6.2 Rust kernel unit-test
- 상세 문서: `docs/rust/02-kernel-unit-test.md`
- `opal-ktest` proc macro crate의 `#[ktest]`로 kernel test 등록 경로를 만듭니다.
- 테스트 엔트리와 결과 출력은 `opal-kernel`에 둡니다.
- `make RUST=1 unit-test QEMU_DISPNONE=1`로 QEMU 테스트를 실행하고 `isa-debug-exit`로 자동 종료합니다.
- 테스트 성공/실패 로그 형식은 Rust kernel test 로그 규약으로 고정합니다.

### 6.3 직접 rustc 빌드 전환
- 상세 문서: `docs/rust/03-direct-rustc-build.md`
- Cargo 기반 kernel wrapper를 제거합니다.
- 커널 본체는 `rustc`, 링크는 `mkfiles/config-rust.mk`의 `TOOLSET_LD`를 직접 호출합니다.
- Rust source 경로를 `src/`, `src/arch/`, `src/platform/` 정책으로 정리합니다.
- VSCode rust-analyzer는 `make rust-project.json RUST=1`로 생성한 `rust-project.json`으로 연결합니다.

### 6.4 Arch primitives
- 상세 문서: `docs/rust/04-arch-primitives.md`
- 포트 I/O, RFLAGS, interrupt enable/disable, halt, CPUID, control register, TLB flush, MSR 같은 x86_64 asm primitive를 `arch/`에 둡니다.
- `platform/`은 serial, QEMU debug-exit 같은 장치 의미만 소유합니다.
- GDT/IDT/TSS, FPU/SIMD, context switch, syscall/interrupt dispatch는 이후 단계에서 다룹니다.

### 6.5 Spin mutexes
- 상세 문서: `docs/rust/05-spin-mutexes.md`
- memory manager와 interrupt/task 포팅 전에 사용할 `spin::Mutex`와 `irqspin::Mutex`를 추가합니다.
- `spin::Mutex`는 CPU 간 공유 상태 보호용으로 사용합니다.
- `irqspin::Mutex`는 local IRQ를 끈 뒤 spin mutex를 획득해 interrupt handler와 공유될 수 있는 상태를 보호합니다.

### 6.6 Memory manager
- 상세 문서: `docs/rust/06-memory-manager.md`
- memory map 정규화와 section map 생성을 포팅합니다.
- paging, PFN, buddy, kmalloc, slab, vmap 순서로 옮깁니다.
- allocator 안정화 후 `alloc` crate를 활성화합니다.

### 6.7 IRQ, timer, task
- 상세 문서: `docs/rust/07-irq-timer-task.md`
- IDT/GDT/TSS와 exception/IRQ dispatch를 Rust 경로로 옮깁니다.
- PIT/timer와 scheduler tick 경로를 연결합니다.
- task, process, coroutine, wait primitive를 단계적으로 포팅합니다.

### 6.8 Drivers and filesystems
- 상세 문서: `docs/rust/08-drivers-filesystems.md`
- UART, PS/2, framebuffer, HID, PATA를 포팅합니다.
- disk, block device, partition, FAT, VFS를 포팅합니다.
- kernel shell 명령은 Rust 커널 내부 명령으로 재구성합니다.

### 6.9 기본 경로 전환
- 상세 문서: `docs/rust/09-default-rust-kernel.md`
- Rust 커널의 주요 부팅/테스트/파일시스템 기능을 완성하면 기본 `make` 경로 전환 여부를 별도 결정합니다.

## 7. 검증 명령
Rust 빌드 골격:
```bash
make RUST=1 CONFIG=debug PLATFORM=pc-x64
make RUST=1 CONFIG=release PLATFORM=pc-x64
make RUST=1 iso CONFIG=debug PLATFORM=pc-x64
make RUST=1 run QEMU_DISPNONE=1 CONFIG=debug PLATFORM=pc-x64
```

Rust kernel unit-test:
```bash
make RUST=1 unit-test QEMU_DISPNONE=1 CONFIG=debug PLATFORM=pc-x64
make RUST=1 unit-test QEMU_DISPNONE=1 CONFIG=release PLATFORM=pc-x64
```

## 8. 결정 사항
- Rust toolchain은 nightly로 고정합니다.
- `opal-kernel`은 커널 본체를 소유합니다.
- `opalkrnl`은 링크용 서브프로젝트이며 Rust crate가 아닙니다.
- `opalkrnl`에는 런타임 코드, 커널 심볼, 부트 ASM을 넣지 않습니다.
- `kmain`과 부트 ASM은 `opal-kernel` 쪽에 둡니다.
- Rust unit-test는 kernel `#[ktest]`로 QEMU에서 실행합니다.
- 커널 본체 빌드는 Cargo가 아니라 직접 `rustc`/`TOOLSET_LD`로 수행합니다.
- 각 포팅 단계는 `docs/rust/nn-some-step-name.md` 상세 문서를 먼저 작성합니다.
