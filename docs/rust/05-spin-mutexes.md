# 05 Spin Mutexes

이 단계는 Rust 커널의 초기 동기화 primitive로 `spin::Mutex`와 `irqspin::Mutex`를 `opal-kernel::sync`에 추가하는 것을 목표로 합니다.

## 목표
- memory manager와 interrupt/task 포팅 전에 공유 상태 보호 API를 준비합니다.
- API는 `spin` crate의 mutex 계열처럼 RAII guard 중심으로 제공합니다.
- `irqspin::Mutex`는 interrupt handler와 공유될 수 있는 상태를 보호할 때 사용합니다.

## 설계 결정
- public 경로는 `crate::sync::{spin, irqspin}`입니다.
- `spin::Mutex<T>`는 `AtomicBool`과 `UnsafeCell<T>` 기반 non-recursive spin mutex입니다.
- `irqspin::Mutex<T>`는 local IRQ를 먼저 끄고 `spin::Mutex<T>`를 획득합니다.
- `irqspin::MutexGuard` drop은 lock을 먼저 해제한 뒤 이전 IRQ 상태를 복원합니다.
- panic poisoning은 제공하지 않습니다.
- guard를 보유한 상태에서 blocking, wait, yield, scheduler 진입을 하지 않습니다.

## API 범위
- `new`
- `lock`
- `try_lock`
- `is_locked`
- `get_mut`
- `into_inner`
- guard `Deref`/`DerefMut`

`force_unlock` 같은 수동 해제 API는 초기 범위에 넣지 않습니다.

## 작업 목록
- `opal-kernel/src/sync/` 모듈을 추가합니다.
- `spin::Mutex<T>`와 `spin::MutexGuard<'_, T>`를 추가합니다.
- `irqspin::Mutex<T>`와 `irqspin::MutexGuard<'_, T>`를 추가합니다.
- Rust debug/release 빌드와 kernel unit-test 실행 경로로 모듈 통합을 확인합니다.

## 검증 명령
```bash
make RUST=1 CONFIG=debug PLATFORM=pc-x64
make RUST=1 CONFIG=release PLATFORM=pc-x64
make RUST=1 unit-test CONFIG=debug PLATFORM=pc-x64 QEMU_DISPNONE=1
make RUST=1 unit-test CONFIG=release PLATFORM=pc-x64 QEMU_DISPNONE=1
```

## 완료 기준
- `opal-kernel::sync`가 `spin::Mutex`와 `irqspin::Mutex`를 public API로 제공합니다.
- debug/release Rust 커널 빌드가 성공합니다.
- debug/release Rust kernel unit-test 실행 경로가 성공합니다.
