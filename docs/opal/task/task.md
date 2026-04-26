# Task / Scheduler

## 개요
- 구현 위치: `kernel/src/task/task.c`
- 목표: 단일 CPU 기준의 최소 태스크 스케줄링, wait-list 대기/깨우기, 타임아웃 처리
- 동기화: 스케줄러 공유 상태는 `irqlock` 보호 구역에서만 수정
- 코루틴 워커 모델은 [`coroutine.md`](coroutine.md) 참고
- 프로세스/유저 ELF 로딩 경로는 [`process.md`](process.md) 참고

## 핵심 상태
- `TASK_READY`: ready queue에 올라가 스케줄 대상인 상태
- `TASK_RUNNING`: 현재 CPU에서 실행 중인 상태
- `TASK_WAITING`: wait list 또는 timeout을 기다리는 상태
- `TASK_DEAD`: 종료된 상태 (refcount가 0이 되면 해제)

## 주요 자료구조
- `g_sched.ready_queue`: 실행 대기 큐 (`linkedlist`)
- `g_sched.timeout_queue`: timeout 기준 정렬 트리 (`rbtree`)
- `g_sched.tid_tree`: `tid -> task` 조회용 트리 (`rbtree`)
- `g_sched.dead_list`: idle 경로에서 정리할 종료 태스크 큐

## 초기화 흐름
- `sched_init()`이 커널/idle 태스크를 생성
- `proc_tree_init()` 후 커널 프로세스(`g_kproc`)를 먼저 초기화
- 공통 초기화는 `task_init()`이 담당
- idle 태스크는 `context_init()` 후 ready queue에 진입
- coroutine 워커는 `coroutine_worker_init()`로 함께 시작
- timeout 처리 메시지 핸들러로 `IRQMSG_SCHED_TIMEOUT`를 등록

## 생성/시작 수명주기
- `task_create(proc, entry, priority)`:
  - 태스크를 즉시 READY로 만들지 않고 `TASK_WAITING` 상태로 생성
  - 이 시점에는 스케줄 대상이 아니므로 실행되지 않음
- `task_resume(task)`:
  - `TASK_WAITING` 태스크를 READY로 전이해 실행 가능 상태로 만듦
- `ktask_start(entry, arg, priority)`:
  - 내부적으로 `task_create()` 후 `task_resume()`까지 수행하는 편의 API
  - 커널 태스크를 "생성+시작" 한 번에 사용할 때 권장

## 상태 전이 헬퍼
- `set_running()`: RUNNING 전이
- `set_ready()`: READY 전이 + ready queue 삽입
- `reset_ready()`: READY 큐에서 제거
- `set_wait_for()`: WAITING 전이 + wait list 등록
- `set_wait_timeout()`: WAITING 전이 + wait list 등록 + timeout tree 등록
- `reset_wait_for()`: wait list 연결 해제
- `reset_timeout()`: timeout queue 연결 해제
- `set_dead()`: DEAD 전이

## 대기/타임아웃
- `task_wait(wl, timeout)`:
  - `timeout <= timer_get_tick()`이면 즉시 실패
  - 아니면 현재 태스크를 `wl`에 연결하고 `schedule()` 호출
  - 복귀 후 `has_timeout` 값으로 timeout 여부를 판정
- timeout 값은 절대 tick deadline이다
- `event_wait()`와 `task_join()`도 같은 absolute deadline 규약을 사용한다
- 단, 조건이 이미 만족된 경우에는 deadline이 현재 tick과 같아도 즉시 성공할 수 있다
- timeout 만료는 `schedule()`에서 `IRQMSG_SCHED_TIMEOUT`를 큐잉하고, 실제 해제는 `irqmsg_timeout()`에서 수행

## Wait List / Event
- wait list 구현은 `kernel/include/opal/task/wait_list.h`, 깨우기 로직은 `kernel/src/task/task.c`
- `wait_list_wake_one()`:
  - wait list 앞의 태스크 1개를 꺼내 READY로 전환
  - timeout 등록이 남아 있으면 같이 제거
- `wait_list_wake_all()`:
  - wait list가 빌 때까지 반복해서 깨움
- event 구현은 `kernel/src/task/event.c`
  - `event_signal()`:
    - auto-reset이면 waiter가 있으면 1개만 깨우고 signaled 상태는 남기지 않음
    - waiter가 없으면 `signaled=true`를 남김
  - manual-reset이면 `signaled=true`를 세우고 모든 waiter를 깨움
  - `event_wait()`는 signaled 상태를 먼저 소비 시도하고, 실패하면 `task_wait()`로 블록

## 종료 / join / 해제
- `task_exit()`:
  - current를 DEAD로 전이
  - `join_list`의 waiter를 모두 깨움
  - 자기 자신을 `dead_list`에 넣고 `schedule()`로 전환
- `task_terminate(task)`:
  - READY/WAITING 연결을 해제한 뒤 DEAD 전이
  - `join_list`의 waiter를 모두 깨움
  - 스택 페이지는 즉시 반환
- `task_join(task, timeout)`:
  - 이미 `TASK_DEAD`면 즉시 성공
  - 아니면 대상의 `join_list`에 현재 태스크를 걸고 대기
- `task_release(task)`:
  - refcount 0 + DEAD 조건에서만 `task_free()` 수행

## 참조 소유권 계약
- `task_create()`, `task_from_id()`, `task_retain()`은 모두 참조 1개를 가진 `taskptr_t`를 돌려준다
- 호출자는 더 이상 필요 없을 때 반드시 `task_release()`를 호출해야 한다
- `task_join()`은 종료 대기만 수행하며 참조를 해제하지 않는다
- 이미 종료된 태스크를 `task_join()`으로 즉시 통과한 경우에도 호출자가 직접 `task_release()`를 해야 한다
