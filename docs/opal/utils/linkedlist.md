# linkedlist

## 개요
- 더미 노드(`dummy`)를 사용하는 원형 이중 연결 리스트입니다.
- 빈 리스트는 `dummy.next == dummy.prev == &dummy` 상태입니다.

## 핵심 API
- 초기화/조회:
  - `linkedlist_init(list)`
  - `linkedlist_nil(list)`
  - `linkedlist_is_nil(list, link)`
  - `linkedlist_is_empty(list)`
  - `linkedlist_head(list)`
  - `linkedlist_tail(list)`
- 삽입/제거:
  - `linkedlist_push_front(list, to_insert)`
  - `linkedlist_push_back(list, to_insert)`
  - `linkedlist_pop_front(list)`
  - `linkedlist_pop_back(list)`
  - `linkedlist_insert_before(link, to_insert)`
  - `linkedlist_insert_after(link, to_insert)`
  - `linkedlist_remove(link)`

## 사용 규약
- 같은 노드를 중복 삽입하면 리스트가 손상될 수 있습니다.
- `linkedlist_remove`는 해당 링크가 유효한 리스트에 연결되어 있다는 전제를 가집니다.
- `pop_*`는 비어 있으면 `NULL`을 반환합니다.

## 매크로
- `linkedlist_foreach(ptr, list)`
- `linkedlist_foreach_backward(ptr, list)`
