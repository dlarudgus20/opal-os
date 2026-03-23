# PATA (ATA PIO) Driver

## 개요
- 구현: `kernel/platform/pc-x64/src/drivers/pata.c`
- 공개 헤더: `kernel/platform/pc-x64/include/opal/platform/drivers/pata.h`
- 대상:
  - primary channel: `0x1f0/0x3f6` (IRQ14)
  - secondary channel: `0x170/0x376` (IRQ15)
- 범위:
  - ATA PIO read/write (`READ SECTORS`, `WRITE SECTORS`, `CACHE FLUSH`)
  - LBA28 주소 범위
  - ATAPI 장치는 identify만 하고 I/O 대상에서 제외

## 초기화 및 등록
- `pata_init()`에서 채널 상태와 포트별 요청 큐(`REQ_SLOTS`)를 초기화한다.
- 각 채널은 `NIEN`을 제어하며 identify 단계에서 장치를 탐지한다.
- floating bus(`status=0xff`) 채널은 비활성으로 둔다.
- ATA 디스크는 `block_device`로 등록된다.
  - 이름: `hda`, `hdb`, `hdc`, `hdd`
  - `sector_size`: 512
  - `sector_count`: identify의 LBA28 섹터 수

## 요청 API (block_device 경유)
- PATA 전용 read/write/wait API는 없다.
- I/O는 공용 block device API를 사용한다.
  - `block_device_read(dev, lba, sectors, buf)`
  - `block_device_write(dev, lba, sectors, buf)`
  - `block_device_wait_request(dev, req, timeout, &result)`
- 디바이스 조회:
  - `block_device_count()`
  - `block_device_get(index)`
  - shell에서는 이 `index`를 drive 번호로 사용한다.

## 내부 모델
- 포트당 1개 공유 요청 큐:
  - 같은 포트의 최대 2개 ATA 디바이스가 하나의 큐를 공유한다.
  - 포트당 한 번에 1개 inflight 요청만 처리한다.
- phase 상태기계:
  - `PATA_PHASE_IDLE`
  - `PATA_PHASE_READ_DATA`
  - `PATA_PHASE_WRITE_DATA`
  - `PATA_PHASE_WRITE_DONE`
  - `PATA_PHASE_COMPLETE`
- write 경로:
  - ATA 커맨드는 최대 256섹터 단위로 분할 발행한다 (`SECTOR_COUNT=0`은 256 의미).
  - command 발행 후 첫 sector는 `DRQ` 확인 뒤 즉시 PIO write
  - 요청 전체가 끝나면 `CACHE FLUSH`를 1회 발행한다.

## IRQ / 타이머 연계
- 채널 IRQ 핸들러는 status를 읽고 phase별 데이터 전송/완료 처리를 진행한다.
- 각 inflight 요청은 deadline tick을 가진다.
- `pata_on_timer(now_tick)`가 deadline 초과를 감시해 timeout 실패 처리한다.
- 타이머 호출 위치: `kernel/src/timer.c`

## 제약 및 주의점
- 현재 구현은 LBA48, DMA, NCQ를 지원하지 않는다.
- 채널 복구(reset) 정책은 단순화되어 있어 timeout 이후 복구 전략은 별도 보강이 필요할 수 있다.
