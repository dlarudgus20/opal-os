# ringbuffer

## 개요
- 고정 길이 레코드 링버퍼입니다.
- `count/maxlen`은 바이트가 아니라 "원소 개수" 기준입니다.
- 실제 원소 타입은 매크로(`ringbuffer_push/pop`)의 `type` 인자로 결정됩니다.

## 핵심 API
- `ringbuffer_init(rb, buffer, maxlen)`
- `ringbuffer_is_full(rb)`
- `ringbuffer_is_empty(rb)`
- `ringbuffer_push_index(rb)` / `ringbuffer_pop_index(rb)`
- `ringbuffer_push(rb, type, val)`
- `ringbuffer_pop(rb, type)`

## 사용 규약
- `buffer`는 `maxlen * sizeof(type)` 이상 저장 가능한 메모리여야 합니다.
- `ringbuffer_push_index`는 full 상태에서 assert합니다.
- `ringbuffer_pop_index`는 empty 상태에서 assert합니다.
