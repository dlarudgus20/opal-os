# Build System (Makefile) 상세

이 문서는 루트 [`Makefile`](../Makefile), [`mkfiles/conf.mk`](../mkfiles/conf.mk), [`mkfiles/rules.mk`](../mkfiles/rules.mk), 각 서브프로젝트 `Makefile`이 어떻게 결합되는지 설명합니다.

## 1. 상위 빌드 그래프
루트 [`Makefile`](../Makefile)은 서브프로젝트 디렉터리를 순회합니다.

핵심 타깃:
- `make kernel`: `kernel/` 빌드
- `make iso`: `kernel.sys` + `grub.cfg`로 ISO 생성
- `make run`: QEMU 실행
- `make test`: 각 서브프로젝트 테스트 순회
- `make unit-test`: `UNIT_TEST=1`로 커널 유닛테스트 실행

## 2. 공통 설정: [`mkfiles/conf.mk`](../mkfiles/conf.mk)
[`conf.mk`](../mkfiles/conf.mk)는 빌드 모드와 툴체인, 공통 CFLAGS/LDFLAGS를 결정합니다.

주요 입력 변수:
- `CONFIG`: `debug`/`release`
- `PLATFORM`: 현재 `pc-x64`
- `IS_TEST=1`: hosted 테스트 빌드
- `UNIT_TEST=1`: 커널 내장 유닛 테스트 빌드

모드별 산출 경로:
- 일반: `build/<platform>/<config>`
- 테스트: `build/tests/<platform>/<config>`
- 커널 유닛테스트: `build/unit-test/<platform>/<config>`

`BUILD_DIR_REF`는 "의존 라이브러리 참조 경로"를 안정적으로 맞추기 위한 변수입니다.
예: 커널 UNIT_TEST 빌드에서 참조 라이브러리는 일반 `build/...`를 사용하도록 분리할 수 있습니다.

## 3. 공통 규칙: [`mkfiles/rules.mk`](../mkfiles/rules.mk)
[`rules.mk`](../mkfiles/rules.mk)는 타깃 생성과 테스트 실행의 본체입니다.

### 3.1 소스 자동 수집
- `SOURCE_DIRS := src platform/$(PLATFORM)/src`
- `find`로 `*.c`, `*.asm` 자동 수집
- 객체별 `.d` 파일 생성 후 `-include`로 자동 dependency 반영

### 3.2 타깃 타입
- `TARGET_TYPE := executable | static-lib | shared-lib`
- `executable`: 링크 스크립트 필요 (`LD_SCRIPT`)
- `static-lib`: `ar`로 `.a` 생성
- `shared-lib`: `.so` 생성

### 3.3 테스트 모드 변환
`IS_TEST=1`일 때:
- `executable`/`static-lib` 타깃은 테스트용 `shared-lib`로 변환됨
- `tests/*.cpp`를 `g++` + `gtest`로 링크
- 필요 시 `TEST_DO_NOT_LINK=1`로 테스트 실행 파일에 라이브러리 직접 링크를 생략

### 3.4 라이브러리 참조
- `STATIC_REFS`, `SHARED_REFS`를 받아 하위 프로젝트 산출물을 의존성으로 추가
- 참조 경로는 `BUILD_DIR_REF` 사용:
  - `../<ref>/<BUILD_DIR_REF>/<ref>.a|.so`

## 4. 서브프로젝트 설정 패턴
각 프로젝트 [`Makefile`](../kernel/Makefile)은 보통 다음만 정의합니다.
- `TARGET_NAME`
- `TARGET_TYPE`
- `STATIC_REFS`/`TEST_SHARED_REFS`
- (필요 시) `LDFLAGS_ON_TEST`, `TEST_DO_NOT_LINK`
- 이후 `include [../mkfiles/conf.mk](../mkfiles/conf.mk)` + `include [../mkfiles/rules.mk](../mkfiles/rules.mk)`

예시:
- [`kernel/Makefile`](../kernel/Makefile)
  - 일반 빌드: `libkc` + `libkubsan` 정적 링크
  - 테스트: `libpanicimpl` shared, `--exclude-libs=libkc`

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
# 일반 커널 빌드
make kernel CONFIG=debug PLATFORM=pc-x64

# 릴리즈 빌드
make kernel CONFIG=release

# 전체 테스트
make test CONFIG=debug PLATFORM=pc-x64

# 커널 테스트만
make -C kernel test CONFIG=debug PLATFORM=pc-x64

# 커널 유닛테스트 빌드/실행 경로
make unit-test CONFIG=debug PLATFORM=pc-x64
```

## 7. 트러블슈팅
### 7.1 테스트에서 sanitizer/lsan 종료 문제
환경에 따라 LSan이 `ptrace` 제약으로 종료될 수 있습니다.
```bash
ASAN_OPTIONS=detect_leaks=0 make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

### 7.2 테스트 공유 라이브러리 충돌
`libkc`의 libc 유사 심볼 노출로 충돌이 생기면 `--exclude-libs=libkc` 적용 여부를 확인합니다.

### 7.3 의존 산출물 경로 불일치
`BUILD_DIR`와 `BUILD_DIR_REF`가 목적에 맞게 설정되었는지 (`IS_TEST`, `UNIT_TEST`) 확인합니다.
