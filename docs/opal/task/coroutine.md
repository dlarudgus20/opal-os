# Coroutine Worker

## 개요
- 헤더: `kernel/include/opal/task/coroutine.h`
- 구현: `kernel/src/task/coroutine.c`
- 목적:
  - 짧은 비동기 상태기계 작업을 커널 워커 태스크 하나에서 순차 처리
  - 인터럽트 핸들러/호출자 문맥에서 긴 후처리를 분리

## 핵심 타입
- `struct coroutine`
  - `link`: 내부 작업 큐 연결
  - `handler`: 실행 함수 (`co_handler_t`)
  - `state`: `CO_DEFERRED`, `CO_READY`, `CO_DONE`
- `co_handler_t`
  - 시그니처: `co_state_t (*)(struct coroutine *co)`
  - 반환값으로 다음 상태를 결정

## 초기화와 실행
- `sched_init()`에서 `coroutine_worker_init()`이 호출된다.
- 워커는 `task_create(..., TASK_PRIORITY_KERNEL)`로 생성된다.
- 내부 큐가 비어 있으면 이벤트를 기다리고, 작업이 들어오면 순서대로 처리한다.

## 사용 순서
1. 호출자가 `coroutine_init(co, handler)`로 코루틴 객체를 초기화
2. 작업 시작 시 `coroutine_set_ready(co)` 호출
3. 워커가 `handler(co)`를 실행
4. `handler` 반환값:
   - `CO_READY`: 큐에 다시 넣어 재실행
   - `CO_DONE`: 종료 단계로 진입

## `CO_DONE` 처리 계약
- 이 구현에서는 `CO_DONE`이 되면 핸들러를 한 번 더 호출한다.
  - 패턴: `if (co->state == CO_DONE) { cleanup; return CO_DONE; }`
- 즉, 핸들러는 일반 실행 경로와 종료(cleanup) 경로를 모두 처리해야 한다.
- 대표 사용 예시는 `kernel/src/fs/partition.c`의 `co_rescan_handler`, `co_reset_handler`.

## 동기화/컨텍스트
- 작업 큐 조작은 `irqlock`으로 보호된다.
- 핸들러 실행 자체는 락을 풀고 수행된다.
- 워커는 단일 태스크이므로 핸들러는 동시에 여러 번 실행되지 않는다.

## 주의점
- `coroutine_set_ready`는 같은 객체를 중복 enqueue하지 않도록 호출자가 상태를 관리해야 한다.
- 핸들러가 참조하는 컨텍스트(버퍼/completion 등)의 수명은 `CO_DONE` cleanup 호출까지 보장해야 한다.
