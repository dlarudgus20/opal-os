# 01 Build Smoke Boot

이 문서는 Rust 포팅 1단계의 상세 계획과 완료 기준을 기록합니다.
1단계 목표는 Rust 커널 빌드 골격을 만들고 `make RUST=1 run`으로 Rust `kmain` 진입을 확인하는 것입니다.

## 목표
- `opal-kernel` rlib crate와 `opalkrnl` 링크용 crate를 추가합니다.
- `opal-kernel`이 `kmain`, panic handler, 부트 ASM, serial smoke output을 소유합니다.
- `opalkrnl`은 executable ELF 링크를 위한 최소 crate로 유지합니다.
- 루트 `make RUST=1`이 Rust 커널 `kernel.sys`를 사용하게 합니다.

## 설계 결정
- Rust toolchain은 `opalkrnl/rust-toolchain.toml`에서 `nightly-2026-04-14`로 고정합니다.
- Rust target spec은 실제 링크를 수행하는 `opalkrnl/platform/pc-x64/x86_64-unknown-none.json`에 둡니다.
- Cargo의 custom target, `build-std`, `.json` target spec 설정은 `opalkrnl/.cargo/config.toml`에 둡니다.
- `opal-kernel/build.rs`는 `opal-build`의 `NasmBuild`를 사용해 `opal-kernel/src/**/*.asm`을 정적 archive로 조립합니다.
- `opal-kernel/src/platform/<platform>/**/*.asm`은 `PLATFORM`과 일치하는 플랫폼만 조립합니다.
- linker script는 실제 ELF 링크를 수행하는 `opalkrnl`에 둡니다.
- `opalkrnl/src/main.rs`에는 `#![no_std]`, `#![no_main]`, `extern crate opal_kernel;`만 둡니다.
- Cargo 빌드 결과는 `opalkrnl/build/<platform>/<config>/opalkrnl.elf`로 복사하고, `objcopy`로 `opalkrnl.sys`를 만듭니다.

## 작업 목록
- `opalkrnl/rust-toolchain.toml`을 추가합니다.
- `opalkrnl/.cargo/config.toml`을 추가합니다.
- `opalkrnl/platform/pc-x64/x86_64-unknown-none.json`을 추가합니다.
- `opal-build/` crate를 추가합니다.
- `opal-kernel/` crate를 추가합니다.
- `opalkrnl/` crate를 추가합니다.
- `mkfiles/rules-cargo.mk`를 추가합니다.
- 루트 `Makefile`에 `RUST=1` 분기를 추가합니다.

## 검증 명령
```bash
make RUST=1 CONFIG=debug PLATFORM=pc-x64
make RUST=1 CONFIG=release PLATFORM=pc-x64
make RUST=1 iso CONFIG=debug PLATFORM=pc-x64
make RUST=1 run QEMU_DISPNONE=1 CONFIG=debug PLATFORM=pc-x64
```

기존 C 경로 회귀 확인:
```bash
make CONFIG=debug PLATFORM=pc-x64
ASAN_OPTIONS=detect_leaks=0 make test CONFIG=debug PLATFORM=pc-x64
```

## 완료 기준
- `make RUST=1 CONFIG=debug PLATFORM=pc-x64`가 `opalkrnl/build/pc-x64/debug/opalkrnl.sys`를 생성합니다.
- `make RUST=1 iso CONFIG=debug PLATFORM=pc-x64`가 Rust 커널을 ISO에 포함합니다.
- `make RUST=1 run QEMU_DISPNONE=1 CONFIG=debug PLATFORM=pc-x64`에서 serial smoke 메시지를 확인할 수 있습니다.
- `RUST=1`이 없는 기존 C 빌드 경로는 계속 동작합니다.
