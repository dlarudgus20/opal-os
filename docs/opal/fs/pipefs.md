# Pipe FS

## 개요
- 헤더: `kernel/include/opal/fs/pipefs.h`
- 구현: `kernel/src/fs/pipefs.c`
- 목적:
  - `SYS_PIPE`가 반환하는 익명 pipe read/write endpoint 제공
  - VFS의 `inode`/`file` 수명 규약 위에서 pipe 버퍼와 대기 이벤트 관리

## 공개 API
- `pipefs_create()`
  - 새 `struct pipefs`를 만들고 내부 `superblock`/`inode`를 초기화한다.
  - 생성된 inode는 `INODE_PIPE` 플래그를 갖는다.
- `pipefs_open_reader(pipe)`
  - pipe inode를 `OPEN_READ`로 열어 read endpoint 파일을 반환한다.
- `pipefs_open_writer(pipe)`
  - pipe inode를 `OPEN_WRITE`로 열어 write endpoint 파일을 반환한다.

반환된 `struct file *`는 호출자가 `file_release()` 해야 한다.

## 구조
- `struct pipefs`
  - `sb`: pipe 전용 superblock
  - `inode`: pipe endpoint를 여는 단일 inode
  - `rpos`, `wpos`, `count`: ring buffer 상태
  - `readers`, `writers`: 현재 열린 read/write endpoint 수
  - `readable`, `writable`: block된 reader/writer를 깨우는 auto-reset event
  - `buffer[PIPE_BUFLEN]`: pipe 데이터 버퍼

현재 `PIPE_BUFLEN`은 `4000` 바이트다.

## I/O 동작
- `read`
  - 요청 크기가 0이면 `0`을 반환한다.
  - 버퍼에 데이터가 있으면 가능한 만큼 읽고, 성공 바이트 수를 반환한다.
  - 버퍼가 비었고 writer가 남아 있으면 `readable`에서 대기한다.
  - 버퍼가 비었고 writer가 없으면 `0`을 반환한다. 이 값이 pipe EOF다.
- `write`
  - 요청 크기가 0이면 `0`을 반환한다.
  - reader가 없으면 아직 쓴 바이트가 없을 때 `OPAL_EIO`를 반환한다.
  - reader가 없지만 일부 바이트를 이미 썼다면 쓴 바이트 수를 반환한다.
  - 버퍼가 가득 찼고 reader가 남아 있으면 `writable`에서 대기한다.
  - 공간이 있으면 가능한 만큼 쓰고, 성공 바이트 수를 반환한다.

Opal에는 아직 signal/EPIPE 모델이 없으므로, reader가 없는 pipe에 대한 write 실패는 현재 `OPAL_EIO`로 표현한다.

## 대기/깨우기
- `readable`과 `writable`은 auto-reset event다.
- 일반 데이터 이동 경로에서는 한 번에 대기자 하나를 깨운다.
  - read가 버퍼 공간을 만들면 writer를 깨운다.
  - write가 데이터를 넣으면 reader를 깨운다.
- 마지막 writer가 닫히면 reader 하나를 깨운다.
- 마지막 reader가 닫히면 writer 하나를 깨운다.
- auto-reset event는 wake-all을 직접 제공하지 않으므로, EOF/EIO를 관찰한 reader/writer가 같은 이벤트를 다시 signal해서 나머지 대기자를 연쇄적으로 깨운다.

이 방식은 현재 event API를 늘리지 않는 최소 구현이다. pipe close 자체가 모든 대기자를 즉시 깨우는 broadcast semantics를 제공하지는 않는다.

## 수명/참조 규약
- `pipefs_create()`는 pipe inode의 초기 참조를 가진 상태로 반환한다.
- endpoint 파일을 열 때 `file_init(..., inode)` 경로가 inode를 retain한다.
- endpoint 파일이 닫히면 reader/writer 카운터를 줄이고 file release 공통 경로가 inode를 release한다.
- `SYS_PIPE` 성공 경로는 read/write endpoint를 FD table에 등록한 뒤 로컬 file 참조와 초기 inode 참조를 해제한다.
- 마지막 inode 참조가 해제되면 pipe inode close 경로에서 `struct pipefs` 전체가 해제된다.

## VFS 연동
- `inode_ops.open`
  - `OPEN_READ` 또는 `OPEN_WRITE`만 허용한다.
  - read/write 동시 지정이나 append/create/truncate 계열 동작은 지원하지 않는다.
- `inode_ops.iterate_dir`, `inode_ops.get_child`, `inode_ops.create_child`
  - pipe inode는 디렉터리가 아니므로 `OPAL_ENOTDIR`를 반환한다.
- `file_ops`
  - `read`, `write`, `close` 지원
  - `seek`, `truncate` 미지원 (`OPAL_ENOTSUPP`)

현재 사용자 API는 익명 pipe만 노출한다. pipe inode가 VFS 경로에 이름으로 연결되지 않으므로 사용자 공간에서 path 기반 reopen은 할 수 없다. 다만 내부 `inode_ops.open`은 reader/writer 카운터가 0이 된 뒤 다시 endpoint를 여는 것을 금지하지 않는다. 이는 향후 named pipe/FIFO 구현 가능성을 막지 않는 형태다.
