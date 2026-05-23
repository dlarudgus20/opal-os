# opal-os

`opal-os`는 `pc-x64` 타깃의 학습용 운영체제 프로젝트입니다.

## 개발 흐름
- 커널 이미지 빌드 (`kernel.elf`, `kernel.sys`)
- ISO 생성 + QEMU 부팅
- hosted 단위 테스트(`make test`)
- 커널 유닛 테스트(`make unit-test`)

## 요구 도구
- `make build`
  - `make`, `nasm`
  - `x86_64-elf-*` 툴체인 (`gcc`, `objcopy`, `objdump`, `gcc-ar`, `gcc-nm`)
  - `python3` (`tools/` 스크립트 실행용)
- `make iso`
  - `cpio` (`initramfs` 생성용)
  - `grub-mkrescue`, `xorriso`
    - `grub-pc` for BIOS
    - `grub-efi-amd64` for UEFI
- `make run` / `make unit-test`
  - `qemu-system-x86_64`
- `make disk-images`
  - `qemu-img` (`qemu-utils`)
- `make test`
  - hosted `gcc/g++`, `libgtest-dev`

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
# UEFI 펌웨어로 실행
make run UEFI=1
```

### 4) 디스크 이미지 생성(선택)
```bash
make disk-images
```

생성된 `hda.img`, `hdb.img`, `hdd.img`는 `make run` 시 자동 연결됩니다.

## 테스트
### 전체 테스트
```bash
make test
```

### 커널 테스트만
```bash
make -C kernel test
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
