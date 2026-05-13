# Vmap

## 개요
- 헤더: `kernel/include/opal/mm/vmap.h`
- 구현: `kernel/src/mm/vmap.c`
- 목적:
  - 물리 주소 범위를 커널 가상 주소 공간의 vmap 영역에 임시 매핑
  - 페이지 정렬되지 않은 물리 주소도 호출자가 바로 접근할 수 있는 포인터로 노출

## 공개 API
- `mm_vmap_init()`
  - vmap 영역의 free list를 초기화한다.
- `mm_vmap_alloc(pa, size, va_span_out)`
  - `pa`부터 `size` 바이트를 포함하는 page-aligned 물리 범위를 매핑한다.
  - 반환값:
    - 성공: 원래 `pa` offset이 반영된 가상 주소
    - 실패 또는 `size == 0`: `NULL`
  - `va_span_out` 출력값:
    - `va_span_out != NULL`은 assert 전제이다.
    - 실제 매핑된 page-aligned 가상 범위를 반환한다.
    - 반환된 span은 `mm_vmap_free()`에 그대로 넘기는 해제 토큰이다.
    - 실패 시 유효하지 않다.
- `mm_vmap_free(span)`
  - `mm_vmap_alloc()`이 반환한 `va_span_out` span을 해제한다.
  - `span.ptr == NULL` 또는 `span.size == 0`이면 아무 동작도 하지 않는다.

## 호출자 계약
- 호출자는 반드시 `va_span_out`을 전달해야 한다.
- `mm_vmap_alloc()`의 반환 포인터는 접근용 주소이고, 해제용 주소가 아니다.
- 매핑 해제 시에는 성공 시 반환된 `va_span_out` span을 `mm_vmap_free()`에 넘긴다.
