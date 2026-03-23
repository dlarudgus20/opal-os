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
  - ATAPI 장치는 identify만 하고 I/O는 비활성

## 초기화
- `pata_init()`에서 채널/요청 상태를 초기화한다.
- 각 채널은 `NIEN`을 제어하며 identify 단계에서 장치를 탐지한다.
- floating bus(`status=0xff`) 채널은 비활성으로 둔다.
- identify 결과:
  - ATA 디스크: 모델/시리얼/LBA28 섹터 수를 기록
  - ATAPI 시그니처: 장치 존재만 기록하고 데이터 I/O 대상에서 제외

## 요청 API
- 비동기 제출:
  - `pata_read_sectors(index, lba, buf, sectors)`
  - `pata_write_sectors(index, lba, buf, sectors)`
  - `sectors`는 `uint32_t`이며, 단일 요청에서 256섹터를 넘길 수 있다.
  - 반환값: `pata_token_t` (`PATA_INVALID_TOKEN`이면 제출 실패)
- 완료 대기:
  - `pata_wait(token, timeout)`
  - 결과: `PATA_WAIT_IO_OK`, `PATA_WAIT_IO_FAIL`, `PATA_WAIT_TIMEOUT`, `PATA_WAIT_INVALID`

## 내부 모델
- 채널별 요청 큐:
  - ringbuffer 기반 큐(`REQ_SLOTS`)
  - 채널당 한 번에 1개 inflight 요청 처리
- phase 상태기계:
  - `PATA_PHASE_IDLE`
  - `PATA_PHASE_READ_DATA`
  - `PATA_PHASE_WRITE_DATA`
  - `PATA_PHASE_WRITE_FLUSH`
  - `PATA_PHASE_COMPLETE`
- write 경로:
  - 내부적으로 ATA 커맨드는 최대 256섹터 단위로 분할 발행한다 (`SECTOR_COUNT=0`은 256 의미).
  - command 발행 후 첫 sector는 `DRQ` 확인 뒤 즉시 PIO write
  - write 요청 전체가 끝난 뒤 `CACHE FLUSH`를 1회 발행한다.

## IRQ / 타이머 연계
- 채널 IRQ 핸들러는 status를 읽고 phase별 데이터 전송/완료 처리를 진행한다.
- 각 inflight 요청은 deadline tick을 가진다.
- `pata_on_timer(now_tick)`가 deadline 초과를 감시해 timeout 실패 처리한다.
- 타이머 호출 위치: `kernel/src/timer.c`

## 제약 및 주의점
- 현재 구현은 LBA48, DMA, NCQ를 지원하지 않는다.
- 채널 복구(reset) 정책은 단순화되어 있어 timeout 이후 복구 전략은 별도 보강이 필요할 수 있다.
- API 사용자는 `buf` 수명과 정렬/접근 가능 메모리 여부를 완료 시점까지 보장해야 한다.
