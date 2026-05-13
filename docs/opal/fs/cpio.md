# CPIO FS (initramfs)

## 개요
- 헤더: `kernel/include/opal/fs/cpio.h`
- 구현: `kernel/src/fs/cpio.c`
- 목적:
  - 부팅 시 전달된 initramfs(CPIO `newc`) 이미지를 읽기 전용 파일시스템으로 마운트

## 공개 API
- `kerrno_t cpio_mount(void *cpio, size_t len, struct superblock **sb_out);`
  - 성공 시 `sb_out`에 마운트 가능한 `superblock`을 반환한다.

## 지원 포맷/타입
- CPIO 헤더 포맷:
  - `070701` (`newc`)
  - `070702` (`crc`, CRC 값 자체는 검증하지 않음)
- 엔트리 타입:
  - 디렉터리(`S_IFDIR`)
  - 일반 파일(`S_IFREG`)
- 그 외 타입은 `OPAL_ENOTSUPP`로 거부

## 동작 특성
- 아카이브 전체를 파싱해 in-memory 트리(`cpio_node`)를 구성
- 파일 데이터는 원본 CPIO 버퍼를 가리키며 복사하지 않음
- `.` 세그먼트는 허용, `..` 세그먼트는 거부
- 동일 경로가 중복되면 마지막 엔트리로 갱신될 수 있음(파서 upsert 동작)

## VFS 연동
- `inode_ops`:
  - `lookup`: 디렉터리 자식 조회 지원
  - `create`: 미지원 (`OPAL_ENOTSUPP`)
- `file_ops`:
  - `seek`, `read` 지원
  - `write`, `truncate` 미지원 (`OPAL_ENOTSUPP`)
- 즉, 현재 `cpiofs`는 읽기 전용입니다.

## 부팅 경로 연동
- `kargs_postboot()`에서 `initramfs` 모듈이 유효하면:
  - `cpio_mount()` 수행
  - `vfs_mount_path(NULL, "/", sb, ...)`로 루트(`/`)에 마운트
- 마운트 실패 시 경고 로그를 남기고 부팅은 계속 진행
