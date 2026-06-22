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
  - `rdi`, `rsi`, `rdx`, `r10`, `r8`: 인자 0~4
- 출력:
  - `rax`: `ret0`
  - `rcx`: `ret1`
  - `r11`: `ret2`

## Syscall 목록
- `SYS_TASK_EXIT`: 현재 태스크를 종료합니다. 성공 반환은 없습니다.
- `SYS_OPEN`: `fd`, `path`, `mode`, `flags`를 받아 파일을 열고 FD에 등록합니다.
- `SYS_CLOSE`: FD 슬롯의 파일 참조를 해제합니다.
- `SYS_DUP`: `oldfd`를 `newfd`로 복제합니다.
- `SYS_STAT`: `fd`가 가리키는 file의 inode flags를 반환합니다.
- `SYS_READ`: `fd`, `buffer`, `size`를 받아 `file_read()` 결과를 반환합니다.
- `SYS_WRITE`: `fd`, `buffer`, `size`를 받아 `file_write()` 결과를 반환합니다.
- `SYS_IOCTL`: `fd`, `op`, `arg`를 받아 `file_ioctl()` 결과를 반환합니다.
- `SYS_MOUNT`: `fstype`, `arg`, `path`를 받아 파일시스템을 마운트합니다. 현재 block device 인자(`arg != 0`)는 지원하지 않습니다.
- `SYS_PIPE`: pipe를 만들고 `ret0=read_fd`, `ret1=write_fd`를 반환합니다.
- `SYS_FORK`: 현재 프로세스를 복제합니다. parent는 child pid, child는 `0`을 반환합니다.
- `SYS_EXEC`: FD로 열린 ELF를 현재 프로세스 이미지로 교체합니다. 성공하면 호출 태스크로 반환하지 않습니다.

## 파일 디스크립터 동작
- FD 소유자는 프로세스(`struct process`)입니다.
- `SYS_OPEN`:
  - 경로를 열어 `struct file *`를 얻고, 지정 FD 또는 빈 FD에 등록합니다.
  - `mode`는 `OPEN_MASK_ALL` 안의 open mode여야 합니다.
  - `flags`는 `OPEN_CREATE`로 새 inode를 만들 때 사용할 `INODE_*` flags입니다.
  - `flags`는 `INODE_MASK_ALL` 안의 inode flags여야 합니다.
  - 성공 시 FD를 반환합니다.
- `SYS_CLOSE`:
  - FD 슬롯의 파일 참조를 해제하고 슬롯을 `NULL`로 비웁니다.
  - 잘못된 FD 또는 이미 닫힌 FD는 실패를 반환합니다.
- `SYS_DUP`:
  - 기존 FD가 가리키는 파일 참조를 지정 FD 또는 빈 FD에 복제합니다.
- `SYS_STAT`:
  - `fd`가 가리키는 file에 inode가 있으면 `INODE_*` flags를 반환합니다.
  - inode가 없는 file이면 `OPAL_ENOTSUPP`를 반환합니다.
- `SYS_READ`:
  - `fd`, `buffer`, `size`를 받아 `file_read()` 결과를 반환합니다.
  - 성공 시 읽은 바이트 수를 반환하고, EOF는 `0`을 반환합니다.
  - 디렉터리 fd에서는 `struct dirent` 레코드 스트림을 반환합니다.
  - 첫 레코드가 buffer에 들어가지 않으면 `OPAL_EBUFSIZE`를 반환합니다.
- `SYS_WRITE`:
  - `fd`, `buffer`, `size`를 받아 `file_write()` 결과를 반환합니다.
  - 성공 시 쓴 바이트 수를 반환합니다.

## 주의사항
- 현재 사용자 주소 범위 검증/안전 복사(`copy_from_user`, `copy_to_user`류)는 미구현입니다.
- `SYS_OPEN`, `SYS_MOUNT`는 문자열을 커널 버퍼로 복사하지만, 사용자 VA 검증은 하지 않습니다.
- `SYS_READ`, `SYS_WRITE`는 사용자 버퍼 포인터를 파일 계층에 직접 전달합니다.
- 잘못된 사용자 포인터나 커널 주소를 넘기는 경우 커널 fault 또는 메모리 손상으로 이어질 수 있습니다.
