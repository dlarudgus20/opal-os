# Testing Guide

## 1. 테스트 종류
1. hosted 테스트 (gtest 기반)
   - `make test`로 실행
   - 각 서브프로젝트 `tests/*.cpp` 실행

2. 커널 유닛 테스트
   - `make unit-test`로 실행
   - 커널 내부의 `DEFINE_UNIT_TEST(...)` 항목 실행

## 2. 전체 테스트
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

## 4. sanitizer
`debug` hosted 테스트는 ASAN+UBSAN 조합을 사용합니다.

LSan 제약 환경 대응:
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

## 6. 분석 팁
- `<target>.map`: 섹션/심볼 배치
- `<target>.nm`, `<target>.sym`: 심볼 확인
- `<target>.disasm`, `<obj>.dump`: 코드 경로 확인
