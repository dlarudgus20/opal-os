# arraylist

## 개요
- 바이트 단위 크기(`size`)와 용량(`capacity`)을 가지는 동적 버퍼입니다.
- 원소 타입은 호출자가 `data_size`와 캐스팅으로 관리합니다.
- 메모리 관리는 사용자 제공 allocator(`alloc/dealloc/shrink`)를 사용합니다.

## 핵심 API
- `arraylist_init(list, initial_capacity, pa)`
- `arraylist_reserve(list, new_capacity)`
- `arraylist_resize(list, new_size)`
- `arraylist_shrink_to(list, new_size)`
- `arraylist_push_back(list, data_size)` -> 삽입 위치 포인터 반환
- `arraylist_pop_back(list, data_size)`
- `arraylist_insert(list, pos, data_size)` -> 삽입 위치 포인터 반환
- `arraylist_remove(list, pos, data_size)`

## 사용 규약
- `size`, `capacity`는 원소 개수가 아니라 바이트 단위입니다.
- `arraylist_push_back` / `arraylist_insert`는 `[[nodiscard]]`입니다.
- 범위/오버플로우 위반 시 assert:
  - `push_back`/`insert` 크기 합 오버플로우
  - `insert` 위치 범위 초과
  - `pop_back`/`remove` 범위 초과

## 매크로
- `arraylist_at(list, type, index)`
- `arraylist_foreach(type, ptr, list)`
