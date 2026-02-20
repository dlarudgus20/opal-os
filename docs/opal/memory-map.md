# Memory Map (pc-x64)
## 1. 가상 주소 공간 요약

```text
memory map (bootstrap)

virtual range                                physical range                    note
--------------------------------------------|---------------------------------|----------------------------
[0x00000000 00000000 ~ 0x00000000 00600000)  [0x00000000 ~ 0x00600000)         identity map
[0xffffffff 80000000 ~ 0xffffffff 80800000)  [0x00200000 ~ 0x00a00000)         kernel image + bootstrap window
[0xffffffff 8f000000 ~ 0xffffffff 8f200000)  [__stack_bottom_lba ~ +0x200000)  kernel stack

memory map (after mm_init())

virtual range                                physical range                    note
--------------------------------------------|---------------------------------|----------------------------
[0xffff9000 00000000 ~ 0xffffd000 00000000)        <full memory>               direct map (64TB)
[0xffffe000 00000000 ~ 0xfffff000 00000000)          <dynamic>                 struct page
[0xffffffff 80000000 ~ 0xffffffff 8e000000)  [0x00200000 ~ +kernel size)       kernel image
[0xffffffff 8f000000 ~ 0xffffffff 8f200000)  [__stack_bottom_lba ~ +0x200000)  kernel stack
```

## 2. 메모리 맵 종류

1. boot memory map: 부트로더가 전달한 원본 mmap
2. (canonical) memory map: boot mmap에서 오버랩, 분할영역 등을 제거하고 정렬한 mmap
3. memory section map: 사용가능 메모리 관리용(metadata + usable)

## 3. mmap_init 흐름 (`kernel/src/mm/map.c`)
1. `refine_mmap()`
   - boot mmap 입력을 정렬/겹침 처리
   - `MMAP_ENTRY_USABLE`은 페이지 단위 정렬
   - non-usable 타입은 원래 경계 유지
   - overlap 시 `mmap_entry_overlap_priority()` 기반으로 절단
   - usable끼리 overlap/adjacent는 병합

2. `init_mm_section()`
   - canonical map에서 usable만 추출
   - `__kernel_end_lba` 이하 제외
   - 첫 엔트리를 `MM_SEC_ENTRY_METADATA(len=0)`로 예약

3. 이후 단계
   - `mm_pagetable_init()`은 section map snapshot(usable 구간) 기준으로 direct map 확장
   - `mm_page_init()`은 `PAGES_START_VIRT + pfn * sizeof(struct page)` 형태 메타데이터 배열 구성

## 4. 부트스트랩 페이지 테이블
`boot.asm`의 `tmp_table`(5 pages, `0x5000`)은 64비트 진입과 초기 higher-half 전환용 임시 테이블입니다.

부팅 후 `mm_pagetable_init()`에서 새 PML4를 구성하고 `cr3`를 교체해 런타임 페이지 테이블로 전환합니다.
