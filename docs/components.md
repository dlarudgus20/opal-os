# Components Overview

## 1. 전체 구조
- `kernel`: 실행 이미지 생성, 시스템 초기화, 상위 로직
- `libkc`: 기본 C 유틸/문자열/포맷
- `libcoll`: 자료구조
- `libkubsan`: 커널 UBSAN 런타임
- `libpanicimpl`: hosted 테스트 panic 구현
- `libslab`: slab 실험/테스트용

## 2. 의존 관계 (개략)
- `kernel` -> `libkc`
- `kernel` -> `libkubsan` (일반 빌드)
- `libkubsan` -> `libkc`
- `libcoll` -> `libkc`
- 테스트 모드에서 `kernel/libkc/libcoll` -> `libpanicimpl`(shared)

## 3. kernel 요약
- `kmain`: 초기화 및 UART shell
- `klog`: 레벨 기반 로그 링버퍼
- `mm/map`: boot mmap sanitize
- `test`: UNIT_TEST 러너
- `drivers/uart`, `tty`: 콘솔 출력 입력

## 4. libkc 요약
- `string.c`: mem/str 계열
- `stdlib.c`: 정렬, 정렬 기반 sort
- `fmt.c` + `stdio.c`: 포맷 출력 엔진
- `assert.h`: panic/assert 매크로

## 5. libcoll 요약
- `arraylist`: 동적 배열
- `linkedlist`/`singlylist`: 연결리스트
- `rbtree`: 정렬 트리
- `ringbuffer`: 고정 길이 큐
