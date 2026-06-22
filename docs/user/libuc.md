# libuc

`libuc`는 사용자 프로그램이 링크하는 최소 사용자 공간 라이브러리다.

## 구성
- `_start`: 사용자 프로그램 진입점
- syscall wrapper: `int 0x80` 호출을 C 함수로 감싼다
- 간단한 I/O helper: `putchar`, `getchar`, `puts`, `getline`, `printf`
- panic 출력: `_panic_format`은 메시지를 stdout에 출력한 뒤 `task_exit()`한다

## FD 상수
- `FD_STDIN = 0`
- `FD_STDOUT = 1`
- `FD_STDERR = 2`
- `FD_INVALID = -1`: `open`/`dup`에서 빈 FD 할당을 요청할 때 사용한다

## syscall wrapper
- `task_exit()`: 현재 태스크 종료
- `open(fd, path, mode, flags)`: 경로를 열어 지정 FD 또는 빈 FD에 등록
  - `flags`는 `OPEN_CREATE`로 새 inode를 만들 때 사용할 `INODE_*` flags
- `close(fd)`: FD 닫기
- `dup(oldfd, newfd)`: FD 복제
- `stat(fd)`: 열린 file의 inode flags 반환
- `read(fd, buffer, size)`: Linux식 bulk read
- `write(fd, buffer, size)`: Linux식 bulk write
- `ioctl(fd, op, arg)`: 장치별 제어 호출
- `mount(fstype, arg, path)`: 파일시스템 마운트
- `pipe(fds)`: `fds[0]`에 read end, `fds[1]`에 write end 저장
- `fork()`: parent에는 child pid, child에는 0 반환
- `exec(fd)`: 열린 ELF 파일로 현재 프로세스 image 교체

## 파일/디렉터리 타입
- `enum inode_flags`
  - `INODE_NORMAL`: 일반 파일
  - `INODE_DIR`: 디렉터리
  - `INODE_DEV`: 장치 inode
  - `INODE_PIPE`: pipe inode
- `stat(fd)`는 위 flags를 반환한다.

## 디렉터리 읽기
- 디렉터리 fd에 대해 `read(fd, buffer, size)`를 호출하면 `struct dirent` 레코드 스트림을 읽는다.
- `struct dirent`
  - `next_offset`: 다음 레코드까지의 바이트 오프셋. 마지막 레코드는 `0`.
  - `name_len`: 이름 길이. `name[]`은 NUL-terminated가 아니다.
  - `flags`: 엔트리의 `INODE_*` flags.
  - `name[]`: 이름 바이트.
- 첫 레코드가 buffer에 들어가지 않으면 `OPAL_EBUFSIZE`가 반환된다.

## 제한
- 현재 커널은 사용자 포인터 범위 검증과 copy-in/copy-out을 완성하지 않았다. 잘못된 포인터는 커널 fault로 이어질 수 있다.
