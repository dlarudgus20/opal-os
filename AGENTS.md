# AGENTS Guide

이 문서는 `opal-os` 저장소에서 작업하는 인간/에이전트를 위한 운영 가이드입니다.
권장사항 중심으로 작성되어 있으며, 반복적으로 발생한 빌드/테스트 이슈를 빠르게 피하는 데 목적이 있습니다.

## Markdown 규칙
- Ordered list 아래 Unordered list를 중첩할 때, 하위 불릿(`-`)은 상위 항목의 본문 시작 위치와 정렬합니다.
- 즉, 고정 공백 수를 강제하지 않고 “상위 항목 텍스트 시작 컬럼 정렬”을 기준으로 합니다.
- 예시:

```md
1. 상위 항목
   - 하위 항목 A
   - 하위 항목 B

10. 상위 항목
    - 하위 항목 A
    - 하위 항목 B
```

## 적용 범위
- 루트 및 모든 하위 서브프로젝트
- `kernel/`, `libkc/`, `libcoll/`, `libkubsan/`, `libpanicimpl/`, `libslab/`, `mkfiles/`

## 프로젝트 맥락
- 타깃: `pc-x64`
- 성격: freestanding 커널 + hosted 테스트(gtest)
- 핵심 산출물:
  - 커널: `kernel/build/.../kernel.elf`, `kernel/build/.../kernel.sys`
  - 테스트: `<subproject>/build/tests/.../test`
  - 커널 유닛테스트: `kernel/build/unit-test/...`

## 표준 명령
### 빌드/실행
```bash
make kernel
make iso
make run
```

### 테스트
```bash
make test
make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

### 커널 유닛테스트
```bash
make unit-test
make clean-unit-test
```

### 정리
```bash
make clean
make clean-test
make fullclean
```

## 작업 원칙 (권장)
1. 변경 전에 영향 범위 확인
   - `rg`, `find`, 기존 `docs/*.md`로 관련 파일을 먼저 확인합니다.

2. 빌드 시스템 변경은 묶어서 점검
   - Makefile을 건드리면 최소한 아래를 함께 확인합니다.
     - `mkfiles/conf.mk`
     - `mkfiles/rules.mk`
   - 해당 서브프로젝트 `Makefile`/`makefile`
   - `BUILD_DIR_REF` 경로 일관성

3. 변경 후 최소 검증
   - 가능한 경우 최소 1개 이상 빌드/테스트를 실행하고 결과를 남깁니다.

4. 큰 구조 변경 시 문서 동반 업데이트
   - `README.md`, `docs/build-system.md`, `docs/testing.md`를 우선 검토합니다.

## 자주 발생한 이슈와 예방
### 1) sanitizer/LSan 환경 이슈
일부 환경에서 LeakSanitizer가 `ptrace` 제약으로 종료될 수 있습니다.
```bash
ASAN_OPTIONS=detect_leaks=0 make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

### 2) 테스트 링크 충돌 (`libkc`)
`libkc`의 libc 유사 심볼 노출로 테스트 링크 충돌 가능성이 있습니다.
- 테스트 링크 플래그에서 `--exclude-libs=libkc` 적용 여부를 확인합니다.

### 3) `UNIT_TEST` + `release` + LTO
`DEFINE_UNIT_TEST` 등록 엔트리가 제거되지 않도록 아래 조합을 유지합니다.
- `__attribute__((used, section(".unittest")))`
- linker script의 `KEEP(*(.unittest))`

### 4) 비-`UNIT_TEST` 빌드 흔적 해석
- `kernel.elf`에는 디버그/심볼 문자열이 남을 수 있습니다.
- 실제 로드 이미지는 `kernel.sys` 기준으로 확인합니다.

## 커널 유닛테스트 작성 가이드
- 테스트 등록: `DEFINE_UNIT_TEST(name)`
- 검증 매크로: `TEST_EXPECT_*`, `TEST_ASSERT_*`
- 실패 모델:
  - `EXPECT_*`: 실패 누적
  - `ASSERT_*`: 현재 테스트 즉시 종료
- 테스트는 작고 독립적으로 유지합니다.

## 서브프로젝트별 포인트
### kernel
- 런타임 동작, hosted 테스트, `UNIT_TEST` 경로를 함께 고려해야 합니다.

### libkc
- libc 호환 이름 함수가 많아 링크 노출 영향에 주의합니다.

### libcoll
- API 계약(assert 전제)과 테스트 케이스를 함께 유지합니다.

### libkubsan
- freestanding UBSAN 핸들러 제공 목적을 유지합니다.

### libpanicimpl
- hosted 테스트에서 panic/assert 심볼 제공이 목적입니다.

### libslab
- 현재는 실험/데모 성격임을 전제로 변경합니다.

## 커밋/PR 전 체크리스트
- [ ] 관련 빌드 최소 1개 성공
- [ ] 관련 테스트 최소 1개 통과
- [ ] 문서 갱신 필요 여부 확인
- [ ] 명령/경로/변수 오타 확인
- [ ] 의도 범위 외 파일 변경 없는지 확인

## 권장 보고 포맷
1. 변경 요약
2. 근거/의도
3. 검증 명령과 결과
4. 남은 리스크/후속 작업

## 참고 문서
- `README.md`
- `docs/README.md`
- `docs/build-system.md`
- `docs/testing.md`
- `docs/kernel-unit-test.md`
- `docs/components.md`
