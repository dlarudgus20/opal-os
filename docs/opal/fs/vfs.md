# VFS

## 개요
- 헤더: `kernel/include/opal/fs/vfs.h`
- 구현: `kernel/src/fs/vfs.c`, `kernel/src/fs/hstr.c`
- 목적:
  - 파일시스템 공통 경로 해석/생성/오픈 계층 제공
  - fs/inode/file 구현과 vfs API 사이의 계약을 통일

## 파일 열기/모드 계약
- `enum open_mode`
  - `OPEN_READ`, `OPEN_WRITE`, `OPEN_APPEND`: 열린 파일의 I/O 모드
  - `OPEN_CREATE`: 없으면 생성
  - `OPEN_NONEXIST`: 이미 있으면 실패
  - `OPEN_TRUNC`: 기존 파일을 열 때 길이 0으로 truncate
- `enum file_mode`
  - `FILE_READ`, `FILE_WRITE`, `FILE_APPEND`: file handle에 저장되는 I/O 권한
  - `FILE_POSLOCK`: 공용 file position을 `pos_mutex`로 보호하는 모드
- `file_read()`/`file_write()`는 `struct file`의 현재 위치를 사용하고, 성공한 바이트 수만큼 위치를 이동한다.
- 각 파일시스템의 `file_ops.read/write`는 음수 errno 또는 성공 바이트 수를 `fs_ssize_t`로 반환한다.
- `file_ioctl()`은 장치별 제어 op를 파일 계층에 전달한다.

## Path Entry
- `struct path_entry`
  - `parent`: 부모 엔트리
  - `inode`: 연결된 inode (`NULL`이면 negative entry)
  - `children`: 자식 엔트리 리스트
  - `name`: `struct hstr` 기반 이름 저장
  - `refcount`: active 참조 카운트
- `path_entry_lookup(pe, name, len, &found)`
  - 캐시에 없으면 `inode->ops->lookup`를 호출해 디렉터리를 재스캔
  - 재스캔 후에도 없으면 negative entry 생성 시도
  - negative 생성 메모리 부족은 `OPAL_ENOMEM`으로 전파
  - `found` 출력값:
    - `OPAL_OK`: positive entry를 retain해 반환
    - `OPAL_ENOENT`: 해당 path가 없는 경우.
      - negative entry가 없다면 생성. 그 후 negative entry를 retain해 반환.
      - `len > VFS_MAX_NAME`일 경우 `found == NULL`
      - inode_ops lookup이 `OPAL_ENOENT` 에러를 줬다면 `found == NULL`
    - 그 외 실패: `found == NULL`
- `path_entry_add(parent, inode, &name, &out)`
  - 같은 이름의 positive entry가 있으면 `OPAL_EEXIST`
  - 같은 이름의 negative entry가 있으면 재사용
  - 성공 시 새 positive entry 또는 재사용된 entry를 retain해 `out`에 반환
- `path_entry_create(pe, flags, mode, &file_out)`
  - `pe->inode == NULL`(negative entry)이면:
    - `OPEN_CREATE`가 없거나 `pe`가 루트라면 `OPAL_ENOENT`
    - `OPEN_CREATE`가 있으면 부모 inode에 대해 `inode_ops.create(parent_inode, pe, flags)` 호출
    - 성공 후 생성된 inode를 `open`하여 `file_out` 반환
  - `pe->inode != NULL`(이미 존재)이면:
    - `OPEN_NONEXIST`가 있으면 `OPAL_EEXIST`
    - `OPEN_TRUNC`가 있으면 기존 inode를 `open` 후 `truncate(0)` 수행
  - 성공 시 열린 file 참조를 `file_out`에 반환하며 호출자가 `file_release` 해야 함

## hstr
- `struct hstr`
  - hashed string: hash값 캐시 + small string optimization
  - short 모드: `in_len != 0xff`, 문자열은 `in_str`에 저장
  - long 모드: `len` 하위 8비트가 `0xff`, 길이는 `len >> 8`, 문자열 포인터는 `str`
- 모드/상수
  - `HSTR_EMPTY`: 길이 0인 short 문자열
  - `HSTR_NULL`: long 포인터가 `NULL`인 실패 값(OOM sentinel)
- 주요 API
  - 생성:
    - `hstr_alloc(len)`: 값을 넣을 수 있는 `hstr` 할당. 문자열 기록 후 `hstr_rehash` 호출해야 함.
    - `hstrdup(cstr)`: C 문자열 복제
    - `hstr_clone(src)`: `hstr` 복제
    - `hstr_stack(ptr, len)`: 외부 버퍼를 non-owning으로 가리키는 임시 `hstr` 생성
      - `hstr_free`로 해제해서는 안 됨.
      - 내부 버퍼가 `'\0'`-terminated 되지 않을 수 있음.
  - 해제:
    - `hstr_free(&hs)`: long 모드만 메모리 해제(short는 no-op).
  - 조회/연산:
    - `hstrget(&hs)`: 버퍼 포인터 반환
    - `hstrlen(&hs)`: 문자열 길이 반환
    - `hstr_rehash(&hs)`: 현재 문자열 기준 hash 재계산
    - `hstr_equal(a, b)`: 문자열 비교
- 소유권/수명 규약
  - `hstrdup/hstr_clone` 결과는 long 모드일 수 있으므로 소유권을 갖는 쪽이 `hstr_free`를 호출해야 함
  - `hstr_stack`은 버퍼를 소유하지 않으며, 원본 버퍼 수명이 끝나면 사용하면 안 됨
  - `hstr_is_null(&hs)`는 생성 실패(OOM) 판별 용도로 사용

## VFS API
- `vfs_get_root()`
  - 루트 `path_entry`를 반환한다.
- `vfs_mount_path(base, path, sb, &mounted)`
  - `vfs_lookup_path(base, path, &found, &unresolved_path)`로 마운트 포인트를 해석한다.
  - `found != NULL`이고 `*unresolved_path == '\0'`일 때 `path_entry_mount_super(found, sb)`를 호출한다.
  - 성공 시 마운트된 path entry를 retain해 `mounted`에 반환하며 호출자가 `path_entry_release` 해야 한다.
- `vfs_lookup_path(base, path, &found, &unresolved_path)`
  - `path`가 `/`로 시작하면 루트 기준으로 탐색
  - 상대 경로는 `base` 기준으로 탐색
  - `found`가 `NULL`이 아니면 retain된 엔트리이며 호출자가 `path_entry_release` 해야 함
  - 반환값:
    - 경로를 끝까지 해석한 경우:
      - `OPAL_OK`
      - `found`: 찾은 path entry
      - `*unresolved_path == '\0'`
    - 중간 컴포넌트에서 miss가 난 경우:
      - `OPAL_ENOENT`
      - `found`: miss가 발생한 negative path entry
      - `unresolved_path`: 아직 해석되지 않은 나머지 경로 시작 위치
    - 중간에서 디렉터리가 아닌 inode를 만난 경우:
      - `OPAL_ENOTDIR`
      - `found`: 해당 non-dir 엔트리
      - `unresolved_path`: 아직 해석되지 않은 나머지 경로 시작 위치
    - inode lookup 실패 시 / 메모리 부족 시:
      - `inode->ops->lookup` 에러코드 그대로 전파 / `OPAL_ENOMEM`
      - `found`: 에러가 발생하기 전 path entry
      - `unresolved_path`: 아직 해석되지 않은 나머지 경로 시작 위치
    - 입력이 invalid(`path[0]=='\0'`, 상대 경로인데 `base==NULL`)인 경우:
      - `OPAL_EINVAL`
      - `found`: `NULL`
      - `unresolved_path`: 입력 `path` 그대로
- `vfs_open_path(base, path, mode, &file_out)`
  - `vfs_create_path(base, path, INODE_NORMAL, mode, &file_out)`의 wrapper이다.
  - 별도의 open-only 경로를 갖지 않는다.
  - 성공 시 열린 file 참조를 `file_out`에 반환하며 호출자가 `file_release` 해야 함
- `vfs_create_path(base, path, flags, mode, &file_out)`
  - 내부 흐름:
    - `vfs_lookup_path(base, path, &found, &unresolved_path)` 호출
    - `found == NULL` 또는 `*unresolved_path != '\0'`이면 lookup 결과 그대로 실패 반환
    - 경로가 완전히 해석되면 `path_entry_create(found, flags, mode, &file_out)` 호출
  - 즉, 생성/기존 파일 처리 정책은 `path_entry_create`가 담당한다.
  - `flags`는 새 inode를 만들 때 parent `inode_ops.create()`로 전달된다.
  - 성공 시 열린 file 참조를 `file_out`에 반환하며 호출자가 `file_release` 해야 함
- `path_entry_open(pe, mode, &file_out)`
  - `path_entry_create(pe, INODE_NORMAL, mode, &file_out)`의 wrapper이다.
- `path_entry_mount_super(pe, sb)`
  - `sb->root`가 유효한 디렉터리 inode여야 한다.
  - `pe->inode != NULL`이면 `OPAL_EBUSY`.
  - 성공 시 `pe->mounted = sb`, `pe->inode = sb->root`.

## 수명/참조 규약
- 트리 연결(`parent->children`)은 active 참조를 의미하지 않는다.
- active 핸들만 `path_entry_retain/release`로 관리한다.
- 구현은 `refcount == 0` 상태의 캐시 엔트리를 허용한다.

## 파일시스템 타입/특수 파일시스템
- `vfs_globals_init()`은 파일시스템 타입 리스트와 전역 devfs(`vfs_globals()->devfs`)를 초기화한다.
- `vfs_fstype_register(fs)`는 이름 기준 중복을 거부하고 파일시스템 타입을 등록한다.
- `vfs_fstype_get(name)`은 등록된 `struct fs_type`을 이름으로 조회한다.
- `devfs`는 block device 없이 마운트되는 전역 `kobjfs` 인스턴스다.
- `kobjfs`는 커널 객체 inode를 디렉터리 트리로 노출하는 특수 파일시스템이다.
- `pipefs`는 `SYS_PIPE`에서 생성되는 익명 pipe inode/file 구현이다. VFS 경로에 이름으로 붙지 않고 FD로만 전달된다.
