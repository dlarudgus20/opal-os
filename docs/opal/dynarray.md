# Dynarray

## 개요
- 헤더: `kernel/include/opal/dynarray.h`
- 구현: `kernel/src/dynarray.c`
- 목적:
  - 커널 내부에서 바이트 단위 가변 버퍼를 제공
  - kmalloc 기반으로 용량 확장/축소를 일관 처리

## 핵심 타입
- `struct dynarray`
  - `data`: 버퍼 시작 주소
  - `size`: 현재 사용 바이트 수
  - `capacity`: 할당 바이트 수

## 주요 API
- `dynarray_init(ar)`
- `dynarray_destroy(ar)`
- `dynarray_reserve(ar, new_capacity)`
- `dynarray_resize(ar, new_size)`
- `dynarray_shrink_to(ar, new_size)`
- `dynarray_push_back(ar, data_size)`
- `dynarray_pop_back(ar, data_size)`
- `dynarray_insert(ar, pos, data_size)`
- `dynarray_remove(ar, pos, data_size)`

## 사용 규약
- `size`/`capacity`는 원소 개수가 아니라 바이트 단위다.
- 범위/오버플로우 위반은 `assert`로 방어한다.
- `new_size == 0` 축소는 내부 버퍼를 해제한다.
- capacity 증가는 kmalloc 경유로 이뤄지며, 실제 할당 용량은 요청값 그대로가 아니라 현재 kmalloc 슬랩/페이지 클래스 정책에 맞춰 결정된다.

## 매크로
- `dynarray_at(list, type, index)`
- `dynarray_foreach(type, ptr, list)`
