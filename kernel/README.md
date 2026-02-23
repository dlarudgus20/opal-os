# kernel

`kernel/`은 opal-os 커널 실행 이미지(`kernel.elf`, `kernel.sys`)를 생성합니다.

## 기능
- opal-os 핵심 커널입니다.

## 빌드
```bash
make -C kernel build CONFIG=debug PLATFORM=pc-x64
```

산출물:
- `build/<platform>/<config>/kernel.elf`
- `build/<platform>/<config>/kernel.sys`

## hosted 테스트
```bash
make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

## 커널 유닛 테스트
```bash
# 프로젝트 루트에서
make unit-test CONFIG=debug PLATFORM=pc-x64
```

유닛테스트 경로 산출물:
- `build/unit-test/<platform>/<config>/...`

## 의존성
- 일반 빌드: `libkc`, `libcoll`, `libkubsan`
- hosted 테스트: `libpanicimpl`(shared)
