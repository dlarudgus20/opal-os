# opal-os

`opal-os`는 `pc-x64` 타깃의 학습용 운영체제 프로젝트입니다.

## 개발 흐름
- 커널 이미지 빌드 (`kernel.elf`, `kernel.sys`)
- ISO 생성 + QEMU 부팅
- hosted 단위 테스트(`make test`)
- 커널 유닛 테스트(`make unit-test`)

## 요구 도구
- `make`, `nasm`
- `x86_64-elf-*` 툴체인 (`gcc`, `objcopy`, `objdump`, `gcc-ar`, `gcc-nm`)
- `qemu-system-x86_64`, `grub-mkrescue`
- hosted 테스트용 `gcc/g++`, `gtest`

## 빠른 시작
### 1) 커널 빌드
```bash
make
```

### 2) ISO 생성
```bash
make iso
```

### 3) 실행
```bash
make run
```

## 테스트
### 전체 테스트
```bash
make test
```

### 커널 테스트만
```bash
make -C kernel test
```

LSan 환경 제약 시:
```bash
ASAN_OPTIONS=detect_leaks=0 make -C kernel test
```

## 커널 유닛 테스트
```bash
make unit-test
```

## make 주요 변수
- `CONFIG=debug|release`
- `PLATFORM=pc-x64`

예시:
```bash
make CONFIG=release
make -C kernel build CONFIG=debug PLATFORM=pc-x64
```

## 정리
```bash
make clean      # 현재 구성 빌드(make) 결과물 정리
make clean-test # 현재 구성 테스트 빌드(make build-test) 결과물 정리
make fullclean  # 모든 빌드 결과물 정리
```

## 문서
더 자세한 정보는 [`docs/`](docs/) 폴더 및 서브프로젝트 `README.md`를 참고하세요.
