# Testing Guide

## 1. 테스트 종류
1. 호스트 테스트 (`gtest`)
   - 각 서브프로젝트 `tests/*.cpp`
   - `IS_TEST=1` 모드에서 실행

2. 커널 내장 유닛 테스트
   - `UNIT_TEST=1`에서 `DEFINE_UNIT_TEST(...)` 항목 실행
   - UART 출력으로 결과 확인

## 2. 전체 테스트 실행
```bash
make test CONFIG=debug PLATFORM=pc-x64
```

## 3. 프로젝트별 테스트
```bash
make -C kernel test CONFIG=debug PLATFORM=pc-x64
make -C libkc test CONFIG=debug PLATFORM=pc-x64
make -C libcoll test CONFIG=debug PLATFORM=pc-x64
make -C libslab test CONFIG=debug PLATFORM=pc-x64
```

## 4. sanitizer 관련
`debug` 테스트 빌드는 기본적으로 ASAN/UBSAN 조합을 사용합니다.

LSan이 환경상 실패하면:
```bash
ASAN_OPTIONS=detect_leaks=0 make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

## 5. 커널 유닛 테스트
```bash
make unit-test CONFIG=debug PLATFORM=pc-x64
```

정리:
```bash
make clean-unit-test CONFIG=debug PLATFORM=pc-x64
```

## 6. 실패 분석 팁
- `<target>.map`: 심볼 배치/중복 확인
- `<target>.nm` / `<target>.sym`: 심볼 존재 여부 확인
- `<target>.disasm` / `<obj>.dump`: 코드 경로 확인
- 테스트 링크 이슈는 `LDFLAGS_ON_TEST`, `TEST_SHARED_REFS`, `TEST_DO_NOT_LINK`부터 확인
