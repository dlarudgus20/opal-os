# Task / Scheduler

## 개요
- 구현 위치: `kernel/src/task/task.c`
- 목표: 단일 CPU 기준의 최소 태스크 스케줄링, 대기/깨우기, 타임아웃 처리
- 동기화: 스케줄러 공유 상태는 `irqlock` 보호 구역에서만 수정

## 핵심 상태
- `TASK_READY`: ready queue에 올라가 스케줄 대상인 상태
- `TASK_RUNNING`: 현재 CPU에서 실행 중인 상태
- `TASK_WAITING`: waitable 또는 timeout을 기다리는 상태
- `TASK_DEAD`: 종료된 상태 (refcount가 0이 되면 해제)

## 주요 자료구조
- `g_sched.ready_queue`: 실행 대기 큐 (`linkedlist`)
- `g_sched.timeout_queue`: timeout 기준 정렬 트리 (`rbtree`)
- `g_sched.tid_tree`: `tid -> task` 조회용 트리 (`rbtree`)
- `g_sched.dead_list`: idle 경로에서 정리할 종료 태스크 큐

## 초기화 흐름
- `sched_init()`이 커널/idle 태스크를 생성
- 공통 초기화는 `task_init()`이 담당
- idle 태스크는 `context_init()` 후 ready queue에 진입
- timeout 처리 메시지 핸들러로 `IRQMSG_SCHED_TIMEOUT`를 등록

## 상태 전이 헬퍼
- `set_running()`: RUNNING 전이
- `set_ready()`: READY 전이 + ready queue 삽입
- `reset_ready()`: READY 큐에서 제거
- `set_waiting()`: WAITING 전이 + wait queue/timeout queue 등록
- `reset_wait_for()`: wait queue 연결 해제
- `reset_timeout()`: timeout queue 연결 해제
- `set_dead()`: DEAD 전이

## 대기/타임아웃
- `task_wait_for(obj, ms)`:
  - `obj->triggered`면 즉시 반환
  - 아니면 현재 태스크를 WAITING으로 전이한 뒤 `schedule()` 호출
  - 복귀 후 `has_timeout` 값으로 timeout 여부를 판정
- `timeout_tick(ms)`:
  - `(uint64_t)ms * TIMER_HZ / 1000`로 계산해 32비트 곱셈 오버플로우 회피
- timeout 만료는 `schedule()`에서 `IRQMSG_SCHED_TIMEOUT`를 큐잉하고, 실제 해제는 `irqmsg_timeout()`에서 수행

## waitable 트리거
- `waitable_trigger(obj)`:
  - `obj->triggered = true`
  - wait queue에서 태스크를 꺼내 READY로 전환
  - auto-reset(`obj->reset=true`)이면 1개 깨운 뒤 `triggered=false`로 복귀

## 종료/해제
- `task_exit()`:
  - current를 DEAD로 전이
  - `dead_list`에 넣고 `schedule()`로 전환
- `task_terminate(task)`:
  - READY/WAITING 연결을 해제한 뒤 DEAD 전이
  - 스택 페이지는 즉시 반환
- `task_release(task)`:
  - refcount 0 + DEAD 조건에서만 `task_free()` 수행
