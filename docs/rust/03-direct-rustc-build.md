# 03 Direct Rustc Build

이 단계는 Cargo wrapper 기반 Rust 커널 빌드를 직접 `rustc`/LLVM lld 호출 구조로 바꾸는 것을 목표로 합니다.

## 목표
- `rules-cargo.mk`와 kernel Cargo wrapper를 제거합니다.
- `rules-rust.mk`를 추가해 Rust 커널 본체를 직접 빌드합니다.
- `opal-kernel`은 Rust object와 ASM object를 합친 `opal-kernel.o`를 생성합니다.
- `opalkrnl`은 `opal-kernel.o`와 Rust sysroot rlib를 최종 ELF로 링크합니다.
- Rust source 경로를 `src/`, `src/arch/`, `src/platform/` 정책으로 정리합니다.

## 설계 결정
- `ARCH`는 `mkfiles/config-rust.mk`에서 `PLATFORM`에 맞춰 결정합니다.
- 현재 `PLATFORM=pc-x64`는 `ARCH=x86_64`만 허용합니다.
- `opal-kernel` Rust 산출 object 이름은 crate root 파일명이 아니라 프로젝트 이름을 사용해 `opal-kernel.rlib.o`로 둡니다.
- `opal-kernel.o`는 `$(TOOLSET_LD) -r`로 `opal-kernel.rlib.o`와 ASM object를 합친 relocatable object입니다.
- 최종 링크도 gcc가 아니라 `mkfiles/config-rust.mk`의 `TOOLSET_LD`를 사용합니다.
- include path 정책은 유지하되 현재는 NASM 입력용으로만 사용합니다.
- VSCode rust-analyzer는 Cargo workspace 대신 `make rust-project.json RUST=1`로 생성한 `rust-project.json`을 사용합니다.
- `rust-project.json`은 local rustup sysroot 경로를 담으므로 추적하지 않습니다.

## 빌드 흐름
1. `rust-sysroot/`가 custom target용 `core`와 `compiler_builtins`를 준비합니다.
2. `opal-ktest/`가 host proc macro dylib를 빌드합니다.
3. `opal-kernel/`이 `src/lib.rs`를 `rustc --crate-type rlib --emit=obj`로 컴파일합니다.
4. `opal-kernel/`이 대상 `src/arch/$(ARCH)/**/*.asm`, `src/platform/$(PLATFORM)/**/*.asm`을 NASM으로 조립합니다.
5. `opal-kernel/`이 Rust object와 ASM object를 `$(TOOLSET_LD) -r`로 `opal-kernel.o`에 합칩니다.
6. `opalkrnl/`이 `opal-kernel.o`, `libcore.rlib`, `libcompiler_builtins.rlib`를 linker script로 최종 링크합니다.
7. `opalkrnl/`이 `objcopy`로 `opalkrnl.sys`를 생성합니다.

## 검증 명령
```bash
make RUST=1 CONFIG=debug PLATFORM=pc-x64
make RUST=1 CONFIG=release PLATFORM=pc-x64
make RUST=1 unit-test CONFIG=debug PLATFORM=pc-x64 QEMU_DISPNONE=1
make RUST=1 unit-test CONFIG=release PLATFORM=pc-x64 QEMU_DISPNONE=1
make RUST=1 iso CONFIG=debug PLATFORM=pc-x64
```

IDE check 경로:
```bash
make rust-project.json RUST=1
```

## 완료 기준
- Cargo 없이 `opal-kernel`과 `opalkrnl` 커널 본체/링크 산출물이 생성됩니다.
- `opalkrnl`에는 Rust crate root나 런타임 코드가 없습니다.
- 변경 없는 두 번째 `make RUST=1`에서 `rustc`/linker/`objcopy`가 다시 실행되지 않습니다.
- `make RUST=1 iso`가 직접 rustc 경로의 `opalkrnl.sys`를 ISO에 포함합니다.
