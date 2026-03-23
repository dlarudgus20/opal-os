# Block Device Layer

## 개요
- 헤더: `kernel/include/opal/fs/block_device.h`
- 구현: `kernel/src/fs/block_device.c`
- 목적:
  - 드라이버별 블록 I/O를 공통 API로 통합
  - 비동기 요청 제출 + 완료 대기 모델 제공
  - shell/상위 계층이 특정 드라이버(PATA 등) API에 직접 의존하지 않도록 분리

## 핵심 타입
- `struct block_device`
  - `ops`: 드라이버 콜백 (`on_request`)
  - `name`: 디바이스 이름 (`lsblk` 출력용)
  - `req_queue`: 요청 큐
  - `sector_size`, `sector_count`: 용량 메타데이터
- `struct block_request`
  - `device`: 요청 소유 디바이스
  - `info`: `lba`, `sectors`, `buffer`, `type(READ/WRITE)`
  - `state`: `QUEUED/INFLIGHT/DONE/RELEASED`
  - `result`: 완료 상태 코드 (`FS_OK` 등)
  - `completion`: 대기/시그널 객체
- `struct block_req_queue`
  - 고정 크기 링 큐 메타데이터 (`wpos`, `fpos`, `rpos`, `count_doing`, `count_done`)

## 공개 API
- 디바이스 등록/조회:
  - `block_device_init(dev, ops)`
  - `block_device_register(dev)`
  - `block_device_count()`
  - `block_device_get(index)`
- 요청 제출:
  - `block_device_read(dev, lba, sectors, buffer)`
  - `block_device_write(dev, lba, sectors, buffer)`
- 완료 처리:
  - `block_device_wait_request(dev, req, timeout, &result)`
  - 내부적으로 완료된 요청은 `block_device_release_request()`로 RELEASED 처리
- 큐 primitive (드라이버 내부 사용):
  - `block_req_queue_init()`
  - `block_req_queue_fetch()`
  - `block_req_queue_pop_fetched()`

## 호출 시점/실행 컨텍스트
- `block_device_read/write` 제출 성공 시, 요청은 먼저 큐에 `QUEUED` 상태로 들어간다.
- 그 직후 `dev->ops->on_request(dev, req)`가 호출된다.
  - 호출 스레드 문맥에서 동기적으로 호출된다.
  - 큐 잠금(`irqlock`)은 해제된 뒤 호출된다.
- 큐가 가득 차서 제출이 실패하면 `NULL`을 반환하며 `on_request`는 호출되지 않는다.

## 동기화/상태 규약
- `block_device_register`, `block_device_count`, `block_device_get`는 내부 `irqlock`으로 보호된다.
- 제출 시 `block_request.device`는 반드시 제출한 `dev`로 세팅된다.
- `block_device_wait_request`/`block_device_release_request`는 `req->device == dev`를 assert로 강제한다.
- 요청 상태 전이:
  - submit: `QUEUED`
  - 드라이버 fetch: `INFLIGHT`
  - 드라이버 완료(pop): `DONE`
  - 상위 release: `RELEASED`
- done request는 release 순서가 엇갈릴 수 있으므로, head부터 `RELEASED`인 항목을 정리하며 `rpos`를 전진시킨다.

## 호출자 계약
- 버퍼 수명:
  - `block_request.info.buffer`는 요청 완료 전까지 유효해야 한다.
  - 즉, `block_device_wait_request(...)=true`로 완료가 확인되기 전에는 버퍼 해제/재사용을 하면 안 된다.
- 요청 핸들 수명:
  - 반환된 `block_request *`는 완료 확인 전까지 보관해야 한다.
  - `block_device_wait_request`가 `true`를 반환하면 내부적으로 release까지 수행되므로, 같은 요청에 대해 추가 release를 호출하면 안 된다.
- timeout 동작:
  - `block_device_wait_request`가 `false`를 반환하면 아직 완료되지 않은 상태다.
  - 취소 API는 없으므로 같은 `req`로 다시 wait하거나 상위에서 재시도 정책을 가져가야 한다.
