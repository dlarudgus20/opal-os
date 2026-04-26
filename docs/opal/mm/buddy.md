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
- `mm_init()`에서 section map을 사용해 `struct buddy`를 생성합니다.
- `buddy_create(buddy, mmap)`는 section map의 `MM_SEC_ENTRY_USABLE` 구간만 free 블록으로 등록합니다.
- `MM_SEC_ENTRY_RESERVED`는 free list에는 넣지 않고 `reserved_pages`로 집계합니다.
- metadata 구간은 buddy free/total 집계에서 제외됩니다.

## 5. 주요 API
- `buddy_create(buddy, mmap)`: 버디 할당기 생성
- `buddy_alloc(buddy, order)`: `2^order` 페이지 할당, 실패 시 `PFN_INVALID`
- `buddy_free(buddy, pfn, order)`: 블록 해제 및 병합
- `buddy_get_free_pages(buddy)`: 현재 free 페이지 수
- `buddy_get_reserved_pages(buddy)`: reserved 페이지 수
- `buddy_get_total_pages(buddy)`: buddy가 관리하는 전체 페이지 수 (initial free + reserved)
- `buddy_get_max_order(buddy)`: 현재 최대 order

## 6. 테스트
- 유닛 테스트: `kernel/unit-tests/mm/buddy.c`
