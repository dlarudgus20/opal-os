# Rust Kernel Unit Test

이 단계는 `make RUST=1 unit-test`에서 Rust 커널 테스트 이미지를 QEMU로 실행하고, 테스트 종료 또는 panic 시 QEMU를 자동 종료하게 만드는 것을 목표로 합니다.

## 1. 설계 결정
- Rust 테스트는 Cargo custom test framework를 사용하지 않습니다.
- `opal-ktest` proc macro crate의 `#[ktest]` attribute로 커널 테스트를 등록합니다.
- 테스트 등록 정보는 `.ktest` 섹션에 배치하고, linker script의 `__ktest_start`/`__ktest_end` 범위를 순회합니다.
- `.ktest`는 기존 linker script의 `.unittest` 영역을 개명한 것이며, 새 영역을 추가하지 않습니다.
- Rust unit-test 빌드는 Cargo `ktest-debug`/`ktest-release` profile을 사용해 일반 `debug`/`release` 산출물과 분리합니다.
- 적용 범위는 Rust unit-test 모드뿐입니다. C 커널 유닛테스트와 일반 `make run`은 기존 동작을 유지합니다.

## 2. 실행 흐름
1. `make RUST=1 unit-test`가 `UNIT_TEST=1`로 Rust 커널을 빌드합니다.
2. `rules-cargo.mk`가 `CONFIG`에 맞는 Cargo `ktest-*` profile과 `--cfg opal_kernel_test`를 적용합니다.
3. `opal-kernel::kmain()`은 `opal_kernel_test` cfg에서 `ktest::run()`을 호출합니다.
4. `ktest::run()`은 `.ktest` 등록 항목을 순회하며 테스트 함수를 실행합니다.
5. 모든 테스트가 통과하면 `isa-debug-exit` 성공 코드로 QEMU를 종료합니다.
6. panic이 발생하면 panic handler가 `isa-debug-exit` 실패 코드로 QEMU를 종료합니다.

Cargo 산출물은 `opalkrnl/target/<cargo-target>/ktest-debug/` 또는 `opalkrnl/target/<cargo-target>/ktest-release/`에 생성됩니다.
따라서 같은 `CONFIG`라도 일반 smoke boot 빌드의 `target/<cargo-target>/debug/` 또는 `target/<cargo-target>/release/`와 테스트 이미지가 겹치지 않습니다.

## 3. 테스트 작성
```rust
use opal_ktest::ktest;

#[ktest]
fn smoke_test() {
    assert_eq!(1, 1);
}
```

`#[ktest]`는 인자와 반환값이 없는 `fn()`에만 사용합니다.
검증 실패는 Rust `#[test]`의 `Result` 반환 모델이 아니라 `assert!`/`assert_eq!` panic으로 표현합니다.

## 4. QEMU 종료 계약
- QEMU 장치: `-device isa-debug-exit,iobase=0xf4,iosize=0x04`
- guest 성공 값: `0x10`
- guest 실패/panic 값: `0x11`
- QEMU raw exit code는 Make recipe에서 일반 테스트처럼 성공 `0`, 실패 `1`로 보정합니다.

## 5. 검증 명령
```bash
make -C opalkrnl CONFIG=debug PLATFORM=pc-x64 UNIT_TEST=1
make -C opalkrnl CONFIG=release PLATFORM=pc-x64 UNIT_TEST=1
make RUST=1 unit-test CONFIG=debug PLATFORM=pc-x64 QEMU_DISPNONE=1
```

성공 시 시리얼 로그에는 다음 형식이 출력되고 QEMU가 자동 종료됩니다.

```text
==== unit test run ====
[ RUN      ] smoke_test
[       OK ] smoke_test
==== unit test end ==== run=1 pass=1 fail=0
```
