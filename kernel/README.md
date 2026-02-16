# kernel

`kernel/`은 opal-os의 커널 실행 이미지(`kernel.elf`, `kernel.sys`)를 만드는 서브프로젝트입니다.

## 역할
- UART 기반 초기 입출력
- 간단한 shell 루프
- 부트 메모리 맵 정리(`construct_usable_map`)
- 커널 로그(`klog`)
- 커널 내장 유닛 테스트 실행기(`unit_test_run`)

## 주요 소스
- [`src/kmain.c`](src/kmain.c): 커널 진입 이후 초기화와 shell
- [`src/klog.c`](src/klog.c): 로그 링버퍼
- [`src/mm/map.c`](src/mm/map.c): boot mmap sanitize
- [`src/test.c`](src/test.c): UNIT_TEST 실행기
- [`platform/pc-x64/src/boot/`](platform/pc-x64/src/boot/): 부트 초기화

## 빌드
```bash
make -C kernel build CONFIG=debug PLATFORM=pc-x64
```

산출물:
- `build/pc-x64/<config>/kernel.elf`
- `build/pc-x64/<config>/kernel.sys`

## 호스트 테스트
```bash
make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

## 커널 유닛 테스트
```bash
make -C kernel build CONFIG=debug PLATFORM=pc-x64 UNIT_TEST=1
```

이 모드에서는 `build/unit-test/...` 경로에 산출물이 생성됩니다.

## 의존성
- 정적: `libkc`
- 일반 빌드에서 추가 정적: `libkubsan`
- 테스트 모드 shared: `libpanicimpl`
