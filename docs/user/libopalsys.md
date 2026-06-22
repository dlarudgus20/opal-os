# libopalsys

`libopalsys`는 사용자 프로그램이 커널 syscall을 사용할 때 링크하는 얇은 시스템 라이브러리다.

## 구성
- `opalsys/opalsys.h`
  - syscall wrapper와 기본 타입/상수
- `opalsys/syscall.h`
  - syscall 번호와 `struct sysret`
- `opalsys/vfs.h`
  - open mode, inode flags, `struct dirent`
- `opalsys/fbcon.h`
  - `/dev/fbcon` ioctl 번호
- `opalsys/hid.h`
  - HID keycode와 `struct hid_char`

## FD/PID 상수
- `FD_STDIN = 0`
- `FD_STDOUT = 1`
- `FD_STDERR = 2`
- `FD_INVALID = -1`: `open`/`dup`에서 빈 FD 할당을 요청할 때 사용한다.
- `PID_INVALID = -1`

## syscall wrapper
- `task_exit()`: 현재 태스크 종료
- `open(fd, path, mode, flags)`: 경로를 열어 지정 FD 또는 빈 FD에 등록
  - `flags`는 `OPEN_CREATE`로 새 inode를 만들 때 사용할 `INODE_*` flags
- `close(fd)`: FD 닫기
- `dup(oldfd, newfd)`: FD 복제
- `stat(fd)`: 열린 file의 inode flags 반환
- `read(fd, buffer, size)`: bulk read
- `write(fd, buffer, size)`: bulk write
- `ioctl(fd, op, arg)`: 장치별 제어 호출
- `mount(fstype, arg, path)`: 파일시스템 마운트
- `pipe(fds)`: `fds[0]`에 read end, `fds[1]`에 write end 저장
- `fork()`: parent에는 child pid, child에는 0 반환
- `exec(fd)`: 열린 ELF 파일로 현재 프로세스 image 교체

## VFS 타입
- `enum inode_flags`
- `enum open_mode`

## 디렉터리 읽기
- 디렉터리 fd에 대해 `read(fd, buffer, size)`를 호출하면 `struct dirent` 레코드 스트림을 읽는다.
- `struct dirent`
  - `next_offset`: 다음 레코드까지의 바이트 오프셋. 마지막 레코드는 `0`.
  - `name_len`: 이름 길이. `name[]`은 NUL-terminated가 아니다.
  - `flags`: 엔트리의 `INODE_*` flags.
  - `name[]`: 이름 바이트.
- 첫 레코드가 buffer에 들어가지 않으면 `OPAL_EBUFSIZE`가 반환된다.

## 장치 ABI
- `/dev/fbcon` ioctl 번호는 `opalsys/fbcon.h`의 `FBCON_IOCTL_*` 상수를 사용한다.
- `/dev/hid` read packet은 `opalsys/hid.h`의 `struct hid_char`를 사용한다.
- 자세한 장치별 의미는 [`../opal/devices.md`](../opal/devices.md)를 기준으로 유지한다.

## libuc와의 관계
- `libopalsys`는 syscall wrapper와 ABI 정의만 제공한다.
- `libuc`는 `_start`, `putchar`, `getline`, `printf`, panic 출력 같은 C runtime/helper를 제공한다.
- 사용자 프로그램에서 `libuc` helper를 쓰려면 두 라이브러리를 함께 링크한다.

## 제한
- 현재 커널은 사용자 포인터 범위 검증과 copy-in/copy-out을 완성하지 않았다.
- 잘못된 사용자 포인터를 syscall wrapper에 넘기면 커널 fault 또는 메모리 손상으로 이어질 수 있다.
