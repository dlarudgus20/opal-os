# singlylist

## 개요
- 더미 헤드(`dummy`) 기반 단일 연결 리스트입니다.
- 제거 연산은 "이전 노드(before)"를 기준으로 동작합니다.

## 핵심 API
- `singlylist_init(list)`
- `singlylist_head(list)`
- `singlylist_before_head(list)`
- `singlylist_push_front(list, to_insert)`
- `singlylist_pop_front(list)`
- `singlylist_insert_after(link, to_insert)`
- `singlylist_remove_after(before)`

## 사용 규약
- `singlylist_remove_after(before)`는 `before->next != NULL`이어야 하며, 위반 시 assert합니다.
- `pop_front`는 비어 있으면 `NULL`을 반환합니다.
- 리스트 내 연결 상태를 호출자가 일관되게 유지해야 합니다.

## 매크로
- `singlylist_foreach(ptr, list)`
- `singlylist_foreach_2(before, ptr, list)`:
  - 순회 중 현재 노드 제거 패턴에서 사용하기 쉽습니다.
