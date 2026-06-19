# Syscall (`int 0x80`)

## 개요
- 사용자 모드 진입점은 `int 0x80` 기반입니다.
- 플랫폼 엔트리:
  - `kernel/platform/pc-x64/src/interrupt.asm`
  - `kernel/platform/pc-x64/src/syscall/syscall.c`
- 커널 디스패처:
  - `kernel/src/syscall/syscall.c`

## 레지스터 ABI
- 입력:
  - `rax`: syscall 번호
  - `rdi`, `rsi`, `rdx`, `r10`, `r8`: 인자 1~5
- 출력:
  - `rax`: `ret1`
  - `rcx`: `ret2`
  - `r11`: `ret3`

## 파일 디스크립터 동작
- FD 소유자는 프로세스(`struct process`)입니다.
- `SYS_OPEN`:
  - 경로를 열어 `struct file *`를 얻고, 프로세스 FD 테이블에 등록합니다.
  - 성공 시 FD를 반환합니다.
- `SYS_CLOSE`:
  - FD 슬롯의 파일 참조를 해제하고 슬롯을 `NULL`로 비웁니다.
  - 잘못된 FD 또는 이미 닫힌 FD는 실패를 반환합니다.
- `SYS_READ`:
  - `fd`, `buffer`, `size`를 받아 `file_read()` 결과를 반환합니다.
  - 성공 시 읽은 바이트 수를 반환하고, EOF는 `0`을 반환합니다.
- `SYS_WRITE`:
  - `fd`, `buffer`, `size`를 받아 `file_write()` 결과를 반환합니다.
  - 성공 시 쓴 바이트 수를 반환합니다.

## 주의사항
- 현재 `SYS_OPEN`의 사용자 포인터 검증/안전 복사(`copy_from_user`류)는 미구현입니다.
- 잘못된 사용자 포인터를 넘기는 경우 커널 fault로 이어질 수 있습니다.
