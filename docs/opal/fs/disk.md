# Disk Layer

## 개요
- 헤더: `kernel/include/opal/fs/disk.h`
- 구현: `kernel/src/fs/disk.c`, `kernel/src/fs/partition.c`
- 목적:
  - 물리 디스크 드라이버(PATA 등)와 상위 block device 계층 사이의 공통 I/O 인터페이스 제공
  - 요청 큐/완료 상태/대기 API를 표준화

## 핵심 타입
- `struct disk`
  - `ops`: 드라이버 콜백 (`on_request`)
  - `name`: 물리 디스크 이름 (예: `hda`)
  - `sectors`: 총 섹터 수
  - `req_queue`: 요청 큐
  - `partition_table`: 메모리에 보관한 MBR 1섹터 버퍼
- `struct disk_request`
  - `info`: `lba`, `sectors`, `buffer`, `type(READ/WRITE)`
  - `state`: `QUEUED/INFLIGHT/DONE/RELEASED`
  - `completion`: `struct fs_completion` (결과 코드 + completion)

## 공개 API
- 디스크 등록/조회:
  - `disk_init(...)`
  - `disk_register(disk)`
  - `disk_list_count()`, `disk_list_get(index)`
- 요청 제출/완료:
  - `disk_read(...)`, `disk_write(...)`
  - `disk_request_wait(req, timeout, &result)`
  - `disk_request_release(req)` (내부적으로 `disk_request_wait`에서 사용)
- 드라이버용 큐 primitive:
  - `disk_req_queue_init(queue, buffer, capacity)`
  - `disk_req_queue_fetch(queue)`
  - `disk_req_queue_pop_fetched(queue, result)`
- 파티션 관리:
  - `disk_register_bdev(disk)`
  - `disk_rescan_partition(disk, completion)`
  - `disk_reset_partition(disk, completion)`

## 요청 상태 전이
- submit: `QUEUED`
- 드라이버 fetch: `INFLIGHT`
- 드라이버 완료(pop): `DONE` + `completion` signal
- 상위 release: `RELEASED`

`disk_request_wait()`는 완료 확인 후 release까지 수행한다.

## 제출/콜백 시점
- `disk_read/write` 제출 성공 시 요청은 먼저 큐에 `QUEUED`로 들어간다.
- 그 직후 `disk->ops->on_request(disk, req)`가 호출된다.
  - 호출은 제출한 스레드 문맥에서 동기적으로 수행된다.
  - 큐 갱신에 사용한 `irqlock`은 해제된 뒤 호출된다.
- 큐가 가득 차 제출이 실패하면 `NULL`을 반환하며 `on_request`는 호출되지 않는다.

## 동기화
- 전역 디스크 목록, 요청 큐 메타데이터는 `irqlock`으로 보호된다.
- `disk_request_wait()`는 completion 대기 후 상태를 재확인한다.
- done request는 release 순서가 엇갈릴 수 있다.
  - `disk_request_release()`는 `rpos` head부터 `RELEASED` 연속 구간을 정리해 큐 공간을 회수한다.

## 파티션 스캔/리셋 모델
- `disk_rescan_partition`:
  - 디스크 LBA0(MBR)를 읽고 파티션 엔트리를 파싱
  - 기존 파티션 block device를 제거 후 새 파티션 block device를 등록
- `disk_reset_partition`:
  - 0으로 채운 MBR 버퍼를 LBA0에 기록
  - 기존 파티션 block device를 제거하고 `partition_table` 버퍼를 교체
- 두 API는 coroutine 워커에서 비동기로 진행되며, `fs_completion`으로 완료를 통지한다.

## 호출자 계약
- 요청 버퍼(`disk_request.info.buffer`)는 완료 전까지 유효해야 한다.
- `disk_request_wait(...)=false`는 timeout/미완료를 의미한다.
- 파티션 API 호출자는 전달한 completion 객체의 수명을 완료 시점까지 보장해야 한다.
