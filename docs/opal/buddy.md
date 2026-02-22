# Buddy Allocator (pc-x64)

## 1. 개요
커널의 페이지 단위 물리 메모리 할당기는 버디(buddy) 알고리즘으로 구현되어 있습니다.
기본 단위는 PFN(page frame number)이며, `order`에 따라 `2^order` 페이지 블록을 할당/해제합니다.

## 2. 핵심 동작
- 할당:
  - 요청 `order` 이상에서 비어있지 않은 free list를 찾음
  - 더 큰 블록을 찾으면 필요한 `order`까지 반복 분할(splitting)
  - 분할 시 남는 절반 블록을 해당 `order` free list에 삽입
- 해제:
  - 같은 `order`의 buddy 블록이 free 상태면 병합(coalescing)
  - 병합 가능한 동안 상위 `order`로 반복
  - 최종 블록을 free list에 삽입

## 3. 메타데이터
- PFN마다 `struct page` 메타데이터를 사용합니다.
- buddy 상태는 `PAGE_FLAG_BUDDY_FREE`, `PAGE_FLAG_BUDDY_HEAD`와 `page->buddy.order`로 관리합니다.
- free list 연결은 `page->buddy.link`를 사용합니다.

## 4. 초기화와 메모리 맵 연동
- `mm_map_init()`이 section map을 구성한 뒤 `mm_pfn_init()`이 PFN 메타데이터를 준비합니다.
- `mm_buddy_init()`은 section map의 usable 구간을 free 블록으로 등록합니다.
- metadata 구간은 buddy에 포함되지 않습니다.

## 5. 주요 API
- `mm_buddy_init()`: 버디 할당기 초기화
- `mm_buddy_alloc(order)`: `2^order` 페이지 할당, 실패 시 `PFN_INVALID`
- `mm_buddy_free(pfn, order)`: 블록 해제 및 병합
- `mm_buddy_get_free_pages()`: 현재 free 페이지 수
- `mm_buddy_get_max_order()`: 현재 최대 order

## 6. 테스트
- 기본 동작 테스트: `kernel/src/mm/buddy_test.c`
- 전수/패턴 테스트: `kernel/src/mm/buddy_test_sweep_all.c`
