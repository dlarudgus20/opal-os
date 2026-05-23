# Rust Build System

이 문서는 `RUST=1` 커널 경로에서 사용하는 빌드 시스템만 설명합니다.

## 1. 루트 타깃
루트 [`Makefile`](../../../Makefile)은 `RUST=1`일 때 Rust 커널 서브프로젝트인 `opalkrnl/`을 선택합니다.

주요 명령:
```bash
make RUST=1 CONFIG=debug PLATFORM=pc-x64
make RUST=1 iso CONFIG=debug PLATFORM=pc-x64
make RUST=1 run QEMU_DISPNONE=1 CONFIG=debug PLATFORM=pc-x64
make RUST=1 unit-test QEMU_DISPNONE=1 CONFIG=debug PLATFORM=pc-x64
make clean RUST=1 CONFIG=debug PLATFORM=pc-x64
make clean-unit-test RUST=1 CONFIG=debug PLATFORM=pc-x64
make fullclean RUST=1
make rust-project.json RUST=1
```

Rust 관련 루트 타깃:
- `build`: `opalkrnl/` 빌드
- `iso`: Rust 커널 `opalkrnl.sys`를 ISO에 포함
- `run`: QEMU 실행
- `unit-test`: `UNIT_TEST=1`, `QEMU_DISPNONE=1`, `QEMU_DEBUG_EXIT=1`로 Rust kernel test 실행
- `clean`: Rust 경로 서브프로젝트와 루트 빌드 결과물 정리
- `clean-root`: 현재 구성의 루트 빌드 결과물만 정리
- `clean-unit-test`: Rust kernel test 빌드 결과물 정리
- `fullclean`: `RUST=1`일 때 Rust 경로 전체 빌드 결과물 정리
- `rust-project.json`: rust-analyzer용 프로젝트 파일 생성

정리 타깃은 선택된 경로 기준으로 동작합니다. Rust 빌드 결과물을 정리하려면 `clean`, `clean-unit-test`, `fullclean`에도 `RUST=1`을 명시합니다.

## 2. Rust 설정 (`mkfiles/config-rust.mk`)
- `CONFIG=debug|release`만 허용합니다.
- `PLATFORM=pc-x64`는 `ARCH=x86_64`로 매핑합니다.
- `ARCH=x86_64`는 `RUST_TARGET_NAME=x86_64-unknown-none`으로 매핑합니다.
- Rust toolchain은 `RUST_TOOLCHAIN ?= nightly-2026-04-14`로 고정합니다.
- Rust target spec은 `arch/$(ARCH)/$(RUST_TARGET_NAME).json`에서 읽습니다.
- 기본 toolset은 `ld.lld-22`, `llvm-objcopy-22`, `llvm-objdump-22`, `llvm-nm-22`, `nasm`입니다.
- `UNIT_TEST=1`이면 `BUILD_DIR`은 `build/unit-test/<platform>/<config>`가 되고 `--cfg opal_ktest`를 추가합니다.
- `TARGET_TYPE=executable` 참조 빌드에서는 `UNIT_TEST`를 export하지 않아 sysroot 참조 경로가 일반 `build/<platform>/<config>`를 가리키게 합니다.

빌드 경로:
- 일반: `build/<platform>/<config>`
- Rust kernel test: `build/unit-test/<platform>/<config>`

## 3. Rust 규칙 (`mkfiles/rules-rust.mk`)
[`mkfiles/rules-rust.mk`](../../../mkfiles/rules-rust.mk)는 Rust 커널 경로 전용 규칙입니다.
커널 본체와 최종 링크에는 Cargo를 사용하지 않고 `rustc`와 `TOOLSET_LD`를 직접 호출합니다.

### 3.1 소스 경로
- 기본 소스: `src/`
- architecture 전용 소스: `src/arch/$(ARCH)/`
- platform 전용 소스: `src/platform/$(PLATFORM)/`
- include 경로: `include/`, `include/arch/$(ARCH)/`, `include/platform/$(PLATFORM)/`

현재 include 경로는 NASM 입력에 사용합니다.

### 3.2 타깃 타입
- `static-obj`
  - Rust crate root를 `rustc --crate-type rlib --emit=obj`로 컴파일합니다.
  - 산출 object 이름은 `$(BUILD_DIR)/$(TARGET_NAME).rlib.o`입니다.
  - ASM object와 함께 `$(TOOLSET_LD) -r`로 `$(BUILD_DIR)/$(TARGET_NAME).o`를 만듭니다.
- `executable`
  - `RUST_OBJ_REFS`의 relocatable object와 Rust sysroot rlib를 linker script로 링크합니다.
  - 최종 ELF는 `$(BUILD_DIR)/$(TARGET_NAME).elf`입니다.

### 3.3 참조 프로젝트
- `RUST_OBJ_REFS`: 최종 링크에 넣을 Rust relocatable object 프로젝트
- `RUST_MACRO_REFS`: `rustc --extern`으로 연결할 host proc macro 프로젝트
- `RUST_SYSROOT_REF`: custom Rust sysroot 프로젝트

`refs` 규칙은 위 참조 프로젝트를 먼저 빌드합니다.

## 4. Rust sysroot
`rust-sysroot/`는 custom target용 `core`와 `compiler_builtins`를 준비합니다.
이 보조 sysroot 준비에는 Cargo `build-std`를 사용하지만, 커널 본체 `opal-kernel/`과 최종 링크 `opalkrnl/`은 Cargo를 사용하지 않습니다.

## 5. 서브프로젝트 Makefile 패턴
`opal-kernel/Makefile`:
```makefile
TARGET_NAME := opal-kernel
TARGET_TYPE := static-obj

RUST_MACRO_REFS := opal-ktest
RUST_SYSROOT_REF := rust-sysroot

all: build

include ../mkfiles/config-rust.mk
include ../mkfiles/rules-rust.mk

build: $(TARGET)
```

`opalkrnl/Makefile`:
```makefile
TARGET_NAME := opalkrnl
TARGET_TYPE := executable

RUST_OBJ_REFS := opal-kernel
RUST_SYSROOT_REF := rust-sysroot

LD_SCRIPT = arch/$(ARCH)/linker.ld
TARGET_STRIPPED = $(BUILD_DIR)/$(TARGET_NAME).sys

all: build

include ../mkfiles/config-rust.mk
include ../mkfiles/rules-rust.mk

build: $(TARGET_STRIPPED)

$(TARGET_STRIPPED): $(TARGET)
	$(TOOLSET_OBJCOPY) -j .startup -j .text -j .rodata -j .data -j .bss -S $< $@
```

## 6. 디버그 산출물
Rust relocatable object와 executable에는 다음 보조 산출물이 생성됩니다.
- `.map`
- `.nm`
- `.sym`
- `.disasm`
- 객체별 `.dump`
