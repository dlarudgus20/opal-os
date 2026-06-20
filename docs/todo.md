# TODO

## Roadmap

- fs
  - fat
  - vfs pentry
  - page cache mmap
  - ramfs
- user mode
- apic

## Planned

1. 리소스 내장 방식 개선
   - 목표: 폰트/리소스 내장 방식을 `#embed`보다 더 나은 대안으로 변경
   - 후보 방식:
     - `objcopy` 또는 `ld -b binary`로 바이너리 섹션 연결

2. `struct event` 등 waitlist 파괴 시 기존 waiter들 처리하면서 graceful destroy

## Known Issues

### FAT/VFS

1. FAT/VFS Deferred Issue
   - `kernel/src/fs/fat/inode.c`의 `fat_inode_readall()`는 FAT 체인 순회 시 방문 상한/사이클 방어가 없다.
   - 디렉터리(또는 size 상한이 사실상 없는 경로)에서 체인 손상으로 사이클이 생기면 무한 순회/과도한 메모리 사용 가능성이 있다.
   - 현재 v0 범위에서는 의도적으로 미해결로 둔다. 이후 안정화 단계에서 `cluster_count` 기반 방문 상한 또는 방문 비트셋 방어를 추가할 것.

2. Partition Locking Deferred Issue
   - `block_device_retain_exclusive` 제거 후 `partition.c` 경로는 일반 `block_device_retain`만 사용한다.
   - 이 상태에서 파티션 제거/재구성 중 `block_device_destroy()` 실패(`refcount != 1`)를 강하게 처리하지 않으면, 파티션 엔트리와 실제 block_device 객체 상태가 불일치할 수 있다.
   - 현재는 의도적으로 `exclusive`를 제거한 실험 상태로 두고, 후속에서 대체 lock 방식(또는 동등한 Busy/rollback 정책) 확정 후 정리한다.

3. FAT 테이블 전체 일괄 로드가 `kzalloc` 상한을 초과해 assert를 유발
   - 위치: `kernel/src/fs/fat/fat_table.c:45`, `kernel/src/fs/fat/fat_table.c:47`
   - 내용: `fat_table_readall()`이 FAT 전체 크기(`fat_sectors * bytes_per_sector`)를 단일 `kzalloc`으로 확보한다.
   - 영향: 파티션이 커지면 `KMALLOC_MAX_SIZE`를 넘겨 `[kzalloc_span()] invalid size` panic이 발생하고, `ls`/`cat` 등 파일 연산이 즉시 중단된다.
   - 전체 버퍼에 모든 내용을 올리는 v0 구현을 바꾸면 자연스럽게 해결될 이슈이다.

4. FAT 디렉터리 생성 실패 시 클러스터 누수
   - 위치: `kernel/src/fs/fat/inode.c:240`, `kernel/src/fs/fat/inode.c:261`, `kernel/src/fs/fat/inode.c:287`
   - 내용: `table_alloc()` 성공 후 부모 dentry 쓰기 실패 시 할당된 `first_cluster`를 FAT에서 해제하지 않고 종료한다.
   - 영향: I/O 오류 경로에서 클러스터 누적으로 장기적으로 `OPAL_ENOSPC`를 유발할 수 있다.
   - 개선점: best-effort 롤백 추가, fsck 필요 로그.

5. 새 디렉터리 초기화 쓰기 실패를 무시하고 성공 반환
   - 위치: `kernel/src/fs/fat/inode.c:275`, `kernel/src/fs/fat/inode.c:276`, `kernel/src/fs/fat/inode.c:279`, `kernel/src/fs/fat/inode.c:281`, `kernel/src/fs/fat/inode.c:285`
   - 내용: `.`/`..`/end-marker dentry 쓰기 결과를 확인하지 않아 일부 실패해도 `OPAL_OK`로 반환한다.
   - 영향: 부분 초기화된 디렉터리가 생성되어 이후 lookup/ls 동작이 비결정적으로 깨질 수 있다.
   - 개선점: ./.. 기록 후 부모 dentry 변경으로 broken directory 방지. fsck 필요 로그.

6. 연속 슬래시 경로에서 빈 이름 negative entry가 생성됨
   - 위치: `kernel/src/fs/vfs.c:136`, `kernel/src/fs/vfs.c:147`, `kernel/src/fs/vfs.c:153`, `kernel/src/fs/vfs.c:154`
   - 내용: `path_entry_lookup()`가 경로 구분자(`/`)를 1개만 건너뛰어 `//` 입력 시 `sep == 0` 상태가 발생하고, lookup 실패 시 길이 0 이름(`""`)의 negative entry를 만든다.
   - 영향: VFS 트리에 빈 이름 엔트리가 누적되어 lookup/ls 결과가 오염될 수 있다.
   - 개선점: 컴포넌트 전환 시 `while (*subpath == '/') subpath++;`로 연속 슬래시를 모두 소비하고, `sep == 0` 컴포넌트는 생성/캐시하지 않도록 방어.

7. FAT mount 시 VBR 시그니처(0x55AA) 검증 누락
   - 위치: `kernel/src/fs/fat/sb.c:136`, `kernel/src/fs/fat/sb.c:141`, `kernel/src/fs/fat/sb.c:37`
   - 내용: `fat_mount()`는 VBR를 읽고 `parse_bpb()`만 수행하며, `vbr.buffer[510..511] == 0x55,0xAA` 확인이 없다.
   - 영향: 손상/비정상 섹터가 BPB 필드만 우연히 맞으면 FAT로 오인 마운트될 수 있다.
   - 개선점: `fat_mount()`에서 `parse_bpb()` 이전에 시그니처를 먼저 검증하고, 불일치 시 `OPAL_ENOENT` 반환.
