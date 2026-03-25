# Block Device Layer

## 개요
- 헤더: `kernel/include/opal/fs/block_device.h`
- 구현: `kernel/src/fs/block_device.c`
- 참고: 요청 큐/완료 상태 머신은 [`disk.md`](disk.md)에서 다룹니다.
- 목적:
  - 물리 디스크(`struct disk`) 위에 파티션/전체 디스크를 같은 형태로 노출
  - 상위 계층(shell, fs)이 디스크 오프셋 계산 없이 LBA 기반 I/O를 사용하도록 분리

## 핵심 타입
- `struct block_device`
  - `disk`: 실제 I/O를 수행할 물리 디스크
  - `name`: 디바이스 이름 (`lsblk` 출력용)
  - `offset`: 디스크 기준 시작 LBA
  - `sectors`: 디바이스가 노출하는 섹터 수
- 요청 타입은 disk 계층(`struct disk_request`)을 그대로 사용

## 공개 API
- 디바이스 등록/삭제:
  - `block_device_register(disk, name, offset, sectors)`
  - `block_device_unregister(dev)`
  - `block_device_unregister_partitions(disk)`
- 디바이스 조회:
  - `bdev_list_count()`
  - `bdev_list_get(index)`
- I/O 제출:
  - `block_device_read(dev, lba, sectors, buffer)`
  - `block_device_write(dev, lba, sectors, buffer)`

## 동작 모델
- `block_device_read/write`는 범위 확인만 수행하고 실제 요청은 `disk_read/write`로 전달한다.
- 실제 완료 대기/상태 전이는 disk 계층이 담당한다.
  - 대기: `disk_request_wait(req, timeout, &result)`
  - 결과: `FS_OK`, `FS_ERR_IO`, `FS_ERR_BUSY`, `FS_ERR_NOMEM` 등

## 동기화/상태 규약
- 등록/삭제/조회(`block_device_register`, `block_device_unregister`, `bdev_list_count`, `bdev_list_get`)는 내부 `irqlock`으로 보호된다.
- `block_device_unregister_partitions(disk)`는 `offset > 0`인 엔트리만 제거한다.
  - 즉, 전체 디스크 엔트리(`offset == 0`)는 유지하고 파티션 엔트리만 재생성하는 데 사용한다.

## 호출자 계약
- 버퍼 수명:
  - `disk_request.info.buffer`는 요청 완료 전까지 유효해야 한다.
  - 즉, `disk_request_wait(...)=true`로 완료가 확인되기 전에는 버퍼 해제/재사용을 하면 안 된다.
- 이름 수명:
  - 파티션 이름은 동적으로 할당되어 `block_device_unregister*`에서 해제된다.
  - 전체 디스크 이름은 정적 문자열을 사용한다.
