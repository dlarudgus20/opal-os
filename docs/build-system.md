# Build System

이 문서는 루트 [`Makefile`](../Makefile), [`mkfiles/conf.mk`](../mkfiles/conf.mk), [`mkfiles/rules.mk`](../mkfiles/rules.mk), 각 서브프로젝트 `Makefile`의 동작을 설명합니다.

## 1. 루트 타깃
루트 [`Makefile`](../Makefile) 주요 타깃:
- `make build`: `kernel/` 빌드
- `make iso`: `kernel.sys` + `grub.cfg` + `initramfs`로 ISO 생성
- `make run`: QEMU 실행
- `make disk-images`: QEMU용 qcow2 디스크 이미지 생성
- `make test`: 각 서브프로젝트 테스트 순회
- `make unit-test`: `UNIT_TEST=1`로 커널 유닛테스트 실행
- `make clean`: 현재 구성 빌드(make) 결과물 정리
- `make clean-test`: 현재 구성 테스트 빌드(make build-test) 결과물 정리
- `make fullclean`: 모든 빌드 결과물 정리

빌드 구성 변수:
- `CONFIG=debug|release`
- `PLATFORM=pc-x64`
- `NO_ANALYZER=1`: freestanding debug 빌드에서 `-fanalyzer`를 비활성화
- `NO_SANITIZE=1`: freestanding debug 빌드에서 `-fsanitize=undefined`를 비활성화

실행 관련 변수:
- `QEMU_FLAGS`: `make run`, `make unit-test`에서 QEMU 실행 인자에 추가
- `QEMU_HDDS`: 루트의 `hda.img`, `hdb.img`, `hdd.img` 존재 여부에 따라 QEMU 디스크 인자 자동 구성
- `QEMU_DISPNONE=1`: `QEMU_FLAGS`에 `-display none`을 자동 추가
- `UEFI=1`: `-bios $(UEFI_FIRMWARE)`를 추가해 UEFI 펌웨어로 실행
- `UEFI_FIRMWARE`: 기본값 `/usr/share/ovmf/OVMF.fd`

### 1.1 ISO / initramfs 생성 경로
- `$(ISO_FILE)` 생성 전 `$(INITRAMFS)`를 먼저 갱신합니다.
- `$(INITRAMFS)` 규칙 동작:
  - 필요한 사용자 프로그램을 빌드
  - `$(INITRAMFS_DIR)`를 먼저 비운 뒤(`rm -rf`) `initramfs/`를 통째로 복사
  - 빌드된 사용자 프로그램을 `$(INITRAMFS_DIR)`에 복사
  - `cpio -H newc`로 `$(BUILD_DIR)/iso/boot/initramfs` 이미지 생성
- 결과적으로 ISO의 `/boot/initramfs`에는 소스 `initramfs/`와 빌드된 사용자 프로그램이 함께 포함됩니다.

### 1.2 디스크 이미지 생성/연결
- `make disk-images`는 루트에 QEMU용 qcow2 디스크 이미지를 생성합니다.
  - `hda.img`: 32M
  - `hdb.img`: 16M
  - `hdd.img`: 8M
- `make run`은 루트에 존재하는 디스크 이미지만 QEMU에 연결합니다.
  - `hda.img`: `-hda hda.img`
  - `hdb.img`: `-hdb hdb.img`
  - `hdd.img`: `-hdd hdd.img`

## 2. 공통 설정 (`mkfiles/conf.mk`)
### C 전처리 매크로
- 일반 빌드: `-DOPAL_CONFIG=... -DOPAL_PLATFORM=...`
- hosted 테스트: `-DOPAL_TEST`
- 커널 유닛테스트 빌드: `-DOPAL_UNIT_TEST`

### analyzer 플래그 적용
- freestanding(`IS_TEST_BUILD != 1`) + `CONFIG=debug`:
  - 기본값으로 `-fanalyzer`를 사용
  - `NO_ANALYZER=1`이면 해당 플래그를 제외
- hosted 테스트(`IS_TEST_BUILD = 1`)에는 적용하지 않음

### sanitizer 플래그 적용
- freestanding(`IS_TEST_BUILD != 1`) + `CONFIG=debug`:
  - 기본값으로 `-fsanitize=undefined`를 사용
  - `NO_SANITIZE=1`이면 해당 플래그를 제외
- hosted 테스트(`IS_TEST_BUILD = 1`) + `CONFIG=debug`:
  - `-fsanitize=address,undefined`를 사용
  - 이 경로는 `NO_SANITIZE`의 영향을 받지 않음

### 빌드 경로
- 일반: `build/<platform>/<config>`
- hosted 테스트: `build/tests/<platform>/<config>`
- 커널 유닛테스트: `build/unit-test/<platform>/<config>`

## 3. 공통 규칙 (`mkfiles/rules.mk`)
### 소스 자동 수집
- `src`, `platform/$(PLATFORM)/src`에서 `*.c`, `*.asm` 자동 탐색
- `.d` dependency 파일 생성 및 `-include`로 반영

### 타깃 타입
- `TARGET_TYPE := executable | static-lib | shared-lib`
- `executable`: 링크 스크립트 필요 (`LD_SCRIPT`)
- `static-lib`: `ar`로 `.a` 생성
- `shared-lib`: `-shared`로 `.so` 생성

### 테스트 모드 동작
- `IS_TEST=1`일 때 `executable`/`static-lib`은 테스트용 `shared-lib` 형태로 빌드
- `tests/*.cpp`는 `g++` + `gtest`로 링크된 `test` 실행 파일 생성
- `TEST_DO_NOT_LINK=1`이면 대상 라이브러리 직접 링크 대신 `-ldl`만 사용

### 참조 라이브러리
- `STATIC_REFS`, `SHARED_REFS`를 받아 하위 프로젝트 산출물을 의존성으로 추가

## 4. 서브프로젝트 설정 패턴
각 프로젝트 [`Makefile`](../kernel/Makefile)은 보통 다음만 정의합니다.
- `TARGET_NAME`
- `TARGET_TYPE`
- `STATIC_REFS`/`SHARED_REFS`/`TEST_SHARED_REFS`
- (필요 시) `LDFLAGS_ON_TEST`, `TEST_DO_NOT_LINK`
- 이후 `include [../mkfiles/conf.mk](../mkfiles/conf.mk)` + `include [../mkfiles/rules.mk](../mkfiles/rules.mk)`

예시:
```makefile
TARGET_NAME := <프로젝트 이름>
TARGET_TYPE := <프로젝트 타입>
STATIC_REFS := <프로젝트 static-lib 참조>

# libkc 사용하는 경우
TEST_SHARED_REFS := libpanicimpl
LDFLAGS_ON_TEST := -Wl,--exclude-libs=libkc

all: build

include ../mkfiles/conf.mk
include ../mkfiles/rules.mk

build: $(TARGET)
```

## 5. 디버그 산출물
대부분 타깃에 다음 보조 산출물이 생성됩니다.
- `.map`
- `.nm`
- `.sym`
- `.disasm`
- 객체별 `.dump`

정적 라이브러리는 `.a.nm`도 생성합니다.

## 6. 자주 쓰는 명령
```bash
make CONFIG=debug PLATFORM=pc-x64
make CONFIG=release PLATFORM=pc-x64
make gen
make clean-gen
make disk-images
make test CONFIG=debug PLATFORM=pc-x64
make -C kernel test CONFIG=debug PLATFORM=pc-x64
make unit-test CONFIG=debug PLATFORM=pc-x64
make unit-test QEMU_DISPNONE=1
```

## 7. 리소스 빌드 패턴
프로젝트별로 `res.mk`를 두고, 소스 빌드 전에 생성 리소스를 의존성으로 거는 패턴을 권장합니다.

권장 구조:
- 생성 대상 목록 변수 정의 (`RESOURCES := ...`)
- 리소스 생성 규칙 작성
- 리소스를 사용하는 소스 타깃에 의존성 추가

예시:
```makefile
RESOURCES := res/gen/example.bin

src/res.c: $(RESOURCES)

res/gen/example.bin: res.mk
	@mkdir -p $(dir $@)
	$(PYTHON) ../tools/some-generator.py $@
```

핵심 포인트:
- 생성 리소스는 `src/res.c`(또는 해당 리소스를 `#embed`하는 소스)의 선행 의존성으로 둡니다.
- 외부 입력 파일이나 설정 변수를 쓰는 경우 의존성에도 명시합니다.
  - 예: `res/gen/example.bin: $(INPUT_FILE) res.mk`
- 생성기는 `tools/` 스크립트로 분리해 재사용 가능하게 유지합니다.

관련 문서:
- [`psfextract.md`](tools/psfextract.md)
- [`kernel/README.md`](../kernel/README.md) (커널 리소스 상세)
