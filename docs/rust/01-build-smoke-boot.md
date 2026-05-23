# 01 Build Smoke Boot

이 문서는 Rust 포팅 1단계의 상세 계획과 완료 기준을 기록합니다.
1단계 목표는 Rust 커널 빌드 골격을 만들고 `make RUST=1 run`으로 Rust `kmain` 진입을 확인하는 것입니다.

## 목표
- `opal-kernel` 커널 본체와 `opalkrnl` 링크 서브프로젝트를 추가합니다.
- `opal-kernel`이 `kmain`, panic handler, 부트 ASM, serial smoke output을 소유합니다.
- `opalkrnl`은 executable ELF 링크만 담당하고 런타임 코드는 갖지 않습니다.
- 루트 `make RUST=1`이 Rust 커널 `kernel.sys`를 사용하게 합니다.

## 설계 결정
- Rust toolchain은 `mkfiles/config-rust.mk`의 `RUST_TOOLCHAIN` 기본값 `nightly-2026-04-14`로 고정합니다.
- Rust target spec은 `arch/x86_64/x86_64-unknown-none.json`에 둡니다.
- 커널 본체는 Cargo가 아니라 `rustc`로 직접 빌드합니다.
- `opal-kernel/src/arch/x86_64/boot.asm`은 NASM으로 object를 만들고, Rust object와 `$(TOOLSET_LD) -r`로 `opal-kernel.o`에 합칩니다.
- linker script는 실제 ELF 링크를 수행하는 `opalkrnl`에 둡니다.
- `opalkrnl`에는 Rust crate root를 두지 않습니다.
- Rust sysroot의 `core`/`compiler_builtins`와 `opal-ktest` proc macro dylib는 보조 빌드로 준비합니다.

## 작업 목록
- `arch/x86_64/x86_64-unknown-none.json`을 추가합니다.
- `rust-sysroot/` 보조 sysroot 빌드를 추가합니다.
- `opal-ktest/` proc macro 빌드를 추가합니다.
- `opal-kernel/` 커널 본체 빌드를 추가합니다.
- `opalkrnl/` 최종 링크 빌드를 추가합니다.
- `mkfiles/rules-rust.mk`를 추가합니다.
- 루트 `Makefile`에 `RUST=1` 분기를 추가합니다.

## 검증 명령
```bash
make RUST=1 CONFIG=debug PLATFORM=pc-x64
make RUST=1 CONFIG=release PLATFORM=pc-x64
make RUST=1 iso CONFIG=debug PLATFORM=pc-x64
make RUST=1 run QEMU_DISPNONE=1 CONFIG=debug PLATFORM=pc-x64
```

## 완료 기준
- `make RUST=1 CONFIG=debug PLATFORM=pc-x64`가 `opalkrnl/build/pc-x64/debug/opalkrnl.sys`를 생성합니다.
- `make RUST=1 iso CONFIG=debug PLATFORM=pc-x64`가 Rust 커널을 ISO에 포함합니다.
- `make RUST=1 run QEMU_DISPNONE=1 CONFIG=debug PLATFORM=pc-x64`에서 serial smoke 메시지를 확인할 수 있습니다.
