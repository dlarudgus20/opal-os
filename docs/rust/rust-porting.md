# Rust Porting Plan

이 문서는 `opal-os` 커널을 Rust로 포팅하기 위한 설계 결정과 단계별 작업 계획을 기록합니다.
초기 구현은 문서와 빌드 골격부터 시작하고, 기존 C 커널은 Rust 커널이 기본 경로가 될 때까지 병행 유지합니다.

## 1. 기본 방향
- Rust 커널은 기존 C 커널과 별도 경로로 만든 뒤 `RUST=1`일 때만 선택합니다.
- `make RUST=1`은 Rust 커널을 빌드하고, ISO 생성 시 Rust 버전 `kernel.sys`를 사용합니다.
- 새 Rust 커널은 Rust와 ASM만 사용합니다.
- 기존 C 커널 함수, `libkc`, `libcoll`에 FFI로 의존하지 않습니다.
- `opsh`는 우선 C 구현을 유지합니다.
- 초기 Rust 지원 플랫폼은 `pc-x64`만 대상으로 합니다.

## 2. 크레이트 구조
루트 형제 디렉터리로 다음 Cargo 서브프로젝트를 둡니다.

- `opal-kernel/`
  - 커널 본체 `rlib` crate입니다.
  - `#![no_std]` 커널 코드, `kmain`, panic handler, kernel test harness, platform ASM artifact를 소유합니다.
  - 기존 `libcoll`에서 필요한 자료구조는 단계적으로 `opal-kernel::utils`로 흡수합니다.
- `opalkrnl/`
  - executable ELF를 만들기 위한 링크용 껍데기 crate입니다.
  - `opal-kernel`을 path dependency로 참조합니다.
  - 커널 로직, 모듈, entry 구현, 부트 ASM을 넣지 않습니다.
  - Cargo가 요구하는 최소 파일이 필요하더라도 런타임 코드와 커널 심볼은 0줄이어야 합니다.

`kmain`은 반드시 `opal-kernel` 쪽에 둡니다.
부트 ASM이 호출하는 C ABI 심볼도 `opal-kernel`이 export합니다.

## 3. 빌드 시스템
- Rust toolchain은 루트 `rust-toolchain.toml`로 nightly를 고정합니다.
- Cargo crate용 공통 규칙으로 `mkfiles/rules-cargo.mk`를 추가합니다.
- `rules-cargo.mk`는 복잡한 빌드 로직을 갖지 않는 Cargo wrapper입니다.
  - `build`: `cargo build` 실행
  - `clean`: 해당 Cargo target/build 출력 정리
  - `unit-test`: Rust kernel `#[test]` 기반 테스트 이미지 빌드/실행
- 기존 `rules.mk`는 C/ASM 프로젝트용으로 유지합니다.
- 루트 `Makefile`은 `RUST=1`일 때 `make -C opalkrnl`을 호출합니다.
- `RUST=1`일 때 `KERNEL_ELF`, `KERNEL_BIN`은 `opalkrnl` 산출물을 가리켜야 합니다.
- `make iso`, `make run`, `make unit-test`의 사용자 인터페이스는 기존과 동일하게 유지합니다.

### 3.1 부트 ASM과 링크
- 기존 NASM 부트 코드는 Rust 포팅 초기에도 유지합니다.
- 부트 ASM 파일과 조립 책임은 `opal-kernel`이 소유합니다.
- `opal-kernel/build.rs`가 NASM을 호출해 부트 오브젝트를 생성합니다.
- `opalkrnl` 링크 단계는 `opal-kernel`이 만든 ASM artifact와 linker script를 사용합니다.
- linker script는 기존 higher-half 배치, `.startup`, `.text`, `.rodata`, `.data`, `.bss`, `.unittest` 섹션 의미를 보존합니다.

## 4. Rust 런타임 정책
- 1단계 런타임은 `core`만 사용합니다.
- `#![no_std]`, `#![no_main]`, panic handler, halt loop를 먼저 구성합니다.
- allocator와 `alloc` crate는 memory manager 포팅 이후 도입합니다.
- 초기 출력은 serial smoke output 수준으로 시작합니다.
- 포맷팅, 문자열, 자료구조는 `opal-kernel` 내부 Rust 구현으로 단계적으로 마련합니다.

## 5. 테스트 정책
- Rust 서브프로젝트의 `unit-test`는 Rust `#[test]` 기반 커널 테스트로 취급합니다.
- 테스트는 hosted `cargo test`가 아니라 QEMU에서 실행되는 kernel test image를 목표로 합니다.
- `make RUST=1 unit-test QEMU_DISPNONE=1`이 공식 실행 경로입니다.
- 기존 C hosted/gtest 테스트와 C 커널 unit-test는 Rust 포팅 중에도 회귀 확인용으로 유지합니다.

## 6. 포팅 단계
각 단계는 구현 전에 `docs/rust/nn-some-step-name.md` 형식의 상세 문서를 먼저 작성합니다.
- `nn`은 두 자리 단계 번호입니다.
- `some-step-name`은 소문자 kebab-case 영문 이름입니다.
- 상세 문서에는 해당 단계의 목표, 설계 결정, 작업 목록, 검증 명령, 완료 기준을 적습니다.
- 단계 구현 중 새로 확정된 결정은 해당 단계 문서와 이 포팅 계획에 함께 반영합니다.

### 6.1 빌드 골격과 smoke boot
- 상세 문서: `docs/rust/01-build-smoke-boot.md`
- `rust-toolchain.toml`을 추가합니다.
- `opal-kernel` rlib crate와 `opalkrnl` executable wrapper crate를 추가합니다.
- `mkfiles/rules-cargo.mk`를 추가합니다.
- 루트 `Makefile`에 `RUST=1` 분기를 추가합니다.
- `opal-kernel`이 `kmain`과 panic handler를 제공합니다.
- `opal-kernel/build.rs`가 부트 ASM을 조립합니다.
- `make RUST=1 run QEMU_DISPNONE=1`로 Rust `kmain` 진입과 serial 출력까지 확인합니다.

### 6.2 Rust kernel unit-test
- 상세 문서: `docs/rust/02-kernel-unit-test.md`
- nightly custom test framework 기반으로 kernel `#[test]` 수집 경로를 만듭니다.
- 테스트 엔트리와 결과 출력은 `opal-kernel`에 둡니다.
- `make RUST=1 unit-test QEMU_DISPNONE=1`로 QEMU 테스트를 실행합니다.
- 테스트 성공/실패 로그 형식은 기존 커널 유닛테스트 로그와 최대한 맞춥니다.

### 6.3 Platform primitives
- 상세 문서: `docs/rust/03-platform-primitives.md`
- 포트 I/O, MSR, CPUID, interrupt enable/disable, halt 같은 platform primitive를 Rust로 옮깁니다.
- panic/log 초기 경로를 정리합니다.
- IRQ lock과 기본 error/type 체계를 도입합니다.

### 6.4 Memory manager
- 상세 문서: `docs/rust/04-memory-manager.md`
- memory map 정규화와 section map 생성을 포팅합니다.
- paging, PFN, buddy, kmalloc, slab, vmap 순서로 옮깁니다.
- allocator 안정화 후 `alloc` crate를 활성화합니다.

### 6.5 IRQ, timer, task
- 상세 문서: `docs/rust/05-irq-timer-task.md`
- IDT/GDT/TSS와 exception/IRQ dispatch를 Rust 경로로 옮깁니다.
- PIT/timer와 scheduler tick 경로를 연결합니다.
- task, process, coroutine, wait primitive를 단계적으로 포팅합니다.

### 6.6 Drivers and filesystems
- 상세 문서: `docs/rust/06-drivers-filesystems.md`
- UART, PS/2, framebuffer, HID, PATA를 포팅합니다.
- disk, block device, partition, FAT, VFS를 포팅합니다.
- kernel shell 명령은 Rust 커널 내부 명령으로 재구성합니다.

### 6.7 기본 경로 전환
- 상세 문서: `docs/rust/07-default-rust-kernel.md`
- Rust 커널이 기존 C 커널의 주요 부팅/테스트/파일시스템 기능을 대체하면 기본 `make` 경로를 Rust로 전환합니다.
- C 커널 경로는 전환 직후 한동안 비교 빌드로 남긴 뒤 제거 여부를 별도 결정합니다.

## 7. 검증 명령
초기 Rust 빌드 골격 구현 후:
```bash
make RUST=1 CONFIG=debug PLATFORM=pc-x64
make RUST=1 iso
make RUST=1 run QEMU_DISPNONE=1
```

Rust kernel unit-test 도입 후:
```bash
make RUST=1 unit-test QEMU_DISPNONE=1
```

기존 C 경로 회귀 확인:
```bash
make CONFIG=debug PLATFORM=pc-x64
ASAN_OPTIONS=detect_leaks=0 make test CONFIG=debug PLATFORM=pc-x64
```

## 8. 결정 사항
- Rust toolchain은 nightly로 고정합니다.
- `opal-kernel`은 커널 본체를 소유하는 `rlib` crate입니다.
- `opalkrnl`은 링크용 껍데기 crate이며 런타임 코드를 넣지 않습니다.
- `kmain`과 부트 ASM은 `opal-kernel` 쪽에 둡니다.
- Rust unit-test는 kernel `#[test]`로 QEMU에서 실행합니다.
- `rules-cargo.mk`는 단순 Cargo wrapper로 유지합니다.
- 각 포팅 단계는 `docs/rust/nn-some-step-name.md` 상세 문서를 먼저 작성합니다.
