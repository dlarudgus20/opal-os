# Slab Allocator (pc-x64)

## 1. 개요
슬랩 할당기는 작은 오브젝트를 페이지 단위 버디 할당 위에서 효율적으로 관리합니다.
호출자가 `struct slab` 캐시를 직접 생성/파괴하는 구조입니다.

## 2. 오브젝트 레이아웃
한 페이지 내부 레이아웃:
- `[struct slab_page][slot 0][slot 1]...`

슬롯 레이아웃:
- `[struct slab_obj_hdr][prefix redzone][payload][suffix redzone]`

특징:
- `struct slab_obj_hdr`는 2바이트 비트필드(`is_free:1`, `next_free:15`)를 사용
- free list는 포인터가 아니라 페이지 시작 기준 16비트 오프셋(`free_head`, `next_free`)을 사용
- `0` 오프셋은 free list 끝(sentinel)
- `payload`는 할당 시 0으로 초기화
- free 상태 payload는 `SLAB_UNUSED_PATTERN`으로 채움
- prefix/suffix redzone은 `SLAB_REDZONE_PATTERN`으로 채워 경계 오염을 탐지

## 3. 페이지 상태 관리
- 캐시는 `partial_pages` 리스트만 유지합니다.
- 할당으로 페이지가 full이 되면 리스트에서 제거합니다.
- full 페이지에서 첫 free가 발생하면 다시 `partial_pages`에 삽입합니다.
- 페이지 `inuse == 0`이면 즉시 buddy에 반환합니다.

## 4. 안전성 체크
- `slab_alloc()`:
  - redzone 무결성 검사
  - unused payload 패턴 검사
- `slab_free()`:
  - 캐시 소유권 검사
  - double free 검사
  - 슬롯 경계/범위 검사
  - 해제 후 payload/redzone 재패턴화

## 5. 주요 API
- `slab_create(struct slab *slab, size_t object_size, size_t object_align)`
- `slab_destroy(struct slab *slab)`
- `void *slab_alloc(struct slab *slab)`
- `slab_free(struct slab *slab, void *ptr)`
- `slab_get_object_size()`, `slab_get_inuse()`, `slab_get_total()`

## 6. kmalloc 연계
- `mm_init()`에서 `buddy_create()` 직후 `kmalloc_init()`을 호출해 슬랩 클래스를 초기화합니다.
- `kmalloc_init()`은 슬랩 클래스를 `32, 64, 128, 256, 512, 1024` 바이트로 생성합니다.
- `kmalloc(size)`에서 `size <= 1024`는 가장 가까운 상위 슬랩 클래스로 올림 할당됩니다.
  - 예: `size=1..32 -> 32B slab`, `size=33..64 -> 64B slab`
- `size > 1024`는 버디 페이지 할당 경로를 사용합니다.
- `kmalloc/kfree` 모두 `size <= MAX_SIZE(PAGE_SIZE * 32)`를 전제로 합니다.

## 7. 테스트
- 테스트 파일: `kernel/src/mm/slab_test.c`
- 주요 시나리오:
  - 정렬 보장
  - 재할당 zeroing
  - 단편화/재사용
  - 인터리브 및 스트레스 패턴
