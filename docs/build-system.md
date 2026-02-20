# Build System

이 문서는 루트 [`Makefile`](../Makefile), [`mkfiles/conf.mk`](../mkfiles/conf.mk), [`mkfiles/rules.mk`](../mkfiles/rules.mk), 각 서브프로젝트 `Makefile`의 동작을 설명합니다.

## 1. 루트 타깃
루트 [`Makefile`](../Makefile) 주요 타깃:
- `make build`: `kernel/` 빌드
- `make iso`: `kernel.sys` + `grub.cfg`로 ISO 생성
- `make run`: QEMU 실행
- `make test`: 각 서브프로젝트 테스트 순회
- `make unit-test`: `UNIT_TEST=1`로 커널 유닛테스트 실행
- `make clean`: 현재 구성 빌드(make) 결과물 정리
- `make clean-test`: 현재 구성 테스트 빌드(make build-test) 결과물 정리
- `make fullclean`: 모든 빌드 결과물 정리

## 2. 공통 설정 (`mkfiles/conf.mk`)
### 입력 변수
- `CONFIG=debug|release`
- `PLATFORM=pc-x64`

### C 전처리 매크로
- 일반 빌드: `-DOPAL_CONFIG=... -DOPAL_PLATFORM=...`
- hosted 테스트: `-DOPAL_TEST`
- 커널 유닛테스트 빌드: `-DOPAL_UNIT_TEST`

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
make test CONFIG=debug PLATFORM=pc-x64
make -C kernel test CONFIG=debug PLATFORM=pc-x64
make unit-test CONFIG=debug PLATFORM=pc-x64
```
