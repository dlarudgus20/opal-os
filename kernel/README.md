# kernel

`kernel/`은 opal-os 커널 실행 이미지(`kernel.elf`, `kernel.sys`)를 생성합니다.

## 기능
- UART 기반 콘솔/shell
- `klog` 로그 링버퍼
- 메모리 맵 정제(`refine_mmap`)와 section map 구성
- higher-half 페이징 초기화
- 커널 유닛 테스트 실행

## 주요 파일
- [`src/kmain.c`](src/kmain.c): 커널 진입/초기화/shell
- [`src/mm/map.c`](src/mm/map.c): 메모리 맵
- [`src/mm/mm.c`](src/mm/mm.c): mm 초기화 오케스트레이션
- [`src/mm/page.c`](src/mm/page.c): PFN 메타데이터(`struct page`)
- [`platform/pc-x64/src/mm/pagetable.c`](platform/pc-x64/src/mm/pagetable.c): 페이지 테이블
- [`src/test.c`](src/test.c): 커널 유닛 테스트 러너

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
- 일반 빌드: `libkc`, `libkubsan`
- hosted 테스트: `libpanicimpl`(shared)
