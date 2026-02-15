# opal-os

opal-os는 `pc-x64` 타깃의 freestanding 커널과 보조 라이브러리들로 구성된 실험용 운영체제 프로젝트입니다.

현재 저장소는 다음 흐름을 중심으로 개발됩니다.
- 커널 이미지 빌드 (`kernel.sys`)
- QEMU 부팅
- 호스트 기반 단위 테스트 (`gtest`)
- 커널 내장 유닛 테스트 (`UNIT_TEST=1`)

## 저장소 구성
- [`kernel/`](kernel/): 커널 본체 (UART shell, klog, 메모리 맵 정리, 유닛 테스트 프레임워크)
- [`libkc/`](libkc/): 커널용 C 라이브러리 성격의 기본 함수들
- [`libcoll/`](libcoll/): 자료구조 라이브러리
- [`libkubsan/`](libkubsan/): 커널용 UBSAN 런타임
- [`libpanicimpl/`](libpanicimpl/): 테스트 환경 panic/assert 구현(shared)
- [`libslab/`](libslab/): slab 관련 실험/테스트용 라이브러리
- [`mkfiles/`](mkfiles/): 공통 빌드 규칙 ([`conf.mk`](mkfiles/conf.mk), [`rules.mk`](mkfiles/rules.mk))
- [`docs/`](docs/): 상세 문서

## 요구 도구
- GNU make
- x86_64-elf 툴체인 (`x86_64-elf-gcc`, `x86_64-elf-objcopy`, ...)
- `nasm`
- `qemu-system-x86_64`
- `grub-mkrescue`
- 호스트 테스트용: `gcc/g++`, `gtest`

## 빠른 시작
### 1) 커널 빌드
```bash
make kernel
```

### 2) ISO 생성
```bash
make iso
```

### 3) QEMU 실행
```bash
make run
```

## 테스트
### 전체 서브프로젝트 테스트
```bash
make test
```

### 커널 테스트만 실행
```bash
make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

환경에 따라 LeakSanitizer가 `ptrace` 제약으로 종료될 수 있습니다. 이 경우:
```bash
ASAN_OPTIONS=detect_leaks=0 make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

## 커널 유닛 테스트
커널 내부 유닛 테스트 빌드/실행:
```bash
make unit-test
```

유닛 테스트 산출물 정리:
```bash
make clean-unit-test
```

## 주요 빌드 변수
- `CONFIG`: `debug`(기본) / `release`
- `PLATFORM`: `pc-x64` (현재 지원)
- `UNIT_TEST=1`: 커널 내장 유닛 테스트 빌드 경로 활성화
- `IS_TEST=1`: 각 서브프로젝트의 호스트 테스트 빌드 모드

예시:
```bash
make kernel CONFIG=release
make -C kernel build CONFIG=debug PLATFORM=pc-x64 UNIT_TEST=1
```

## 정리 타깃
- `make clean`: 현재 설정의 빌드 아티팩트 정리
- `make clean-test`: 테스트 아티팩트 정리
- `make fullclean`: 전체 `build/` 정리

## 문서
- [`docs/README.md`](docs/README.md): 문서 인덱스
- [`docs/build-system.md`](docs/build-system.md): Makefile 시스템 상세
- [`docs/testing.md`](docs/testing.md): 테스트 실행/트러블슈팅
- [`docs/troubleshooting.md`](docs/troubleshooting.md): 자주 발생하는 빌드/테스트 문제 모음
- [`docs/kernel-unit-test.md`](docs/kernel-unit-test.md): 커널 유닛 테스트 프레임워크
- [`docs/components.md`](docs/components.md): 컴포넌트 구조
- [`AGENTS.md`](AGENTS.md): 협업/운영 가이드
