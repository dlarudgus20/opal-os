# FAT 구현 문서 (v0)

## 개요
- 헤더: `kernel/include/opal/fs/fat.h`
- 구현: `kernel/src/fs/fat/sb.c`, `kernel/src/fs/fat/inode.c`, `kernel/src/fs/fat/fat_table.c`
- 목표:
  - block device 위에 FAT12/16/32를 mount/format
  - VFS `inode_ops`/`file_ops` 계약으로 파일/디렉터리 연산 제공

## 공개 API
- `kerrno_t fat_mount(struct block_device *bdev, struct superblock **sb_out);`
  - 성공 시 `sb_out`에 마운트 가능한 `superblock`을 반환한다.
- `kerrno_t fat_format(struct block_device *bdev, struct superblock **sb_out);`
  - 성공 시 포맷 완료 후 마운트 가능한 `superblock`을 `sb_out`에 반환한다.

## 마운트(`fat_mount`)
- 동작:
  - VBR(섹터 0) 1섹터를 읽음
  - BPB를 파싱해 `fat_layout` 생성
  - `fat_table_init` + root inode 초기화 후 `superblock` 반환
- BPB 파싱(`parse_bpb`)에서 검증하는 핵심 조건:
  - bytes-per-sector: 512/1024/2048/4096
  - sectors-per-cluster: 2의 거듭제곱
  - reserved_sectors/num_fats는 0 불가
  - 파티션 총 섹터가 block device 범위를 넘지 않아야 함
  - FAT16/32 분기 및 cluster_count 기반 FAT12/16 판정
- 루트 inode 구성:
  - FAT32: `root_cluster` 기반 일반 inode(`fat_inode`)
  - FAT12/16: 고정 root 영역 기반 root inode(`fat_root_inode`)

## 포맷(`fat_format`)
- 동작:
  - 디바이스 크기에서 추천 `sectors_per_cluster` 선택
  - FAT12/16/32 후보를 순회해 배치 계산(`calc_fat_layout`)
  - BPB/boot sector/FAT 초기값/root 초기 엔트리를 기록
  - 포맷 완료 후 superblock 반환
- 포맷 실패 시 단계별로 `OPAL_E*`를 전파하며 partial state를 완전 복구하지는 않는다.

## 디렉터리/엔트리 조회
- 디렉터리 조회는 `inode->ops->lookup` 경로로 들어온다.
  - root(FAT12/16): 고정 root 영역을 읽어 dentry 순회
  - 일반 inode: cluster chain 전체를 읽어 dentry 순회
- dentry 순회(`dentry_lookup`) 규칙:
  - `name[0] == 0`: end marker, 순회 종료
  - `0xe5`(삭제), `.` 시작, 볼륨 라벨은 스킵
  - 나머지는 `struct path_entry`로 등록
- 캐시 등록 실패 처리:
  - `OPAL_EEXIST`: 이미 캐시에 있으므로 무시하고 계속
  - 그 외(`OPAL_ENOMEM` 등): 즉시 에러 전파

## `fat_root_inode` / `fat_inode` 구현 상세
- 공통 베이스:
  - 두 타입 모두 `fat_inode_base` 레이아웃(`inode`, `file`, `sb`, `buffer`, `buflen`)을 공유한다.
  - `ops`는 `struct fat_inode_ops`로 연결되어 `lookup/create/write_dentry/alloc_dentry`를 제공한다.
- `fat_root_inode` (FAT12/16 root 전용):
  - 필드: `offset`(root 시작 섹터), `entries`(고정 root dentry 개수)
  - 데이터 로드: `root_inode_readall`이 root 영역 전체를 한 번에 메모리로 읽음
  - dentry 쓰기/할당:
    - `root_inode_write_dentry`: root 영역의 특정 dentry를 1섹터 단위로 갱신
    - `root_inode_alloc_dentry`: 빈 slot(`name[0] == 0 || 0xe5`) 탐색
  - 파일 인터페이스는 디렉터리로 취급되어 read/write/truncate가 `OPAL_EISDIR`
- `fat_inode` (일반 파일/디렉터리):
  - 필드: `parent`, `dentry_idx`, `first_cluster`, `filesize`
  - 초기화: `fat_inode_init`이 parent dentry를 보고 디렉터리 플래그를 결정
  - 데이터 로드: `fat_inode_readall`이 FAT chain을 따라 전체 내용을 읽어 캐시
  - I/O:
    - `fat_file_read`: 캐시에서 범위 읽기
    - `fat_file_write`: append/non-append, 필요 시 chain 확장
    - `fat_file_truncate`: 현재 `size==0`만 지원
  - dentry 쓰기/할당:
    - `fat_inode_write_dentry`: 디렉터리 데이터 영역 내 dentry를 파일 write 경로로 갱신
    - `fat_inode_alloc_dentry`: 빈 slot 탐색, 없으면 end에 zero dentry append
- 수명 관리 차이:
  - `fat_inode_close`는 일반 child inode를 `kfree`한다.
  - `sb->root32`는 `fat_sb` 내부 인스턴스라 `kfree`하지 않는다.

## 생성/이름 정책
- 생성은 `inode_ops.create(parent_inode, pe, flags)`로 처리된다.
- 이름 변환(`pack_filename`)은 `pe->name`을 FAT 8.3으로 변환:
  - 허용: `A-Z`, `0-9`, `!#$%&'()-@^_`{}~`
  - 소문자/기타 문자는 `OPAL_EINVAL`
  - base 8자 + 확장자 3자 제한
- 디렉터리 생성 시:
  - 부모 dentry에 새 항목 기록
  - child inode 초기화
  - `.`/`..`/end-marker 엔트리 기록 시도(현재 write 실패는 무시)

## 파일 데이터 경로
- 읽기:
  - `fat_inode_readall`이 cluster chain을 끝까지 읽어 inode buffer에 올림
  - `fat_file_read`는 해당 buffer에서 복사
- 쓰기:
  - append/non-append 지원
  - append 시 필요하면 FAT chain을 확장(`fat_table_append`)
  - 쓰기 후 dentry(first cluster/file_size) 갱신
- truncate:
  - 현재 `size == 0`만 지원
  - dentry를 먼저 0 크기/0 cluster로 갱신
  - FAT chain을 순회하며 엔트리 해제

## FAT table 구현
- FAT 캐시는 `fat_table_readall`에서 FAT 전체를 메모리에 올린다.
- FAT12/16/32 각각 `table_at/table_set/table_alloc` 구현이 분리되어 있음.
- FAT 반영 write는 모든 FAT copy(`num_fats`)에 동일 범위를 기록한다.
- `fat_table_append`:
  - 현재 cluster가 EOF여야 새 cluster를 붙임
  - 중간 실패 시 새 cluster 엔트리 0으로 되돌리려 시도
  - 성공 시 새 cluster 번호를 `new_cluster`에 반환

## 제한사항/주의점
- long filename(LFN) 미지원 (8.3만 지원)
- FAT 전체/파일 전체를 메모리에 올리는 경로가 있어 큰 파티션/큰 파일에서 메모리 압박이 큼
- 일부 실패 경로(디렉터리 초기화 write 실패, create 중간 롤백)는 best-effort 수준
- 최신 known issue 목록은 `docs/todo.md`의 FAT 항목을 기준으로 유지한다.
