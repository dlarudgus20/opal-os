# Page Cache / Demand Paging 작업 계획

상태: 초안

대상 브랜치: `codex/vfs-rework`

## 1. 목표

이번 작업은 단순한 buffered read 캐시 추가가 아니라 VM과 VFS 사이에 페이지
단위 계약을 만드는 것을 목표로 합니다.

- 복구 가능한 page fault 처리
- anonymous demand paging
- inode 기반 page cache
- page cache를 통한 buffered file I/O
- demand-paged ELF segment
- file-backed page 공유
- `fork()`의 copy-on-write
- dirty page writeback 및 제한적 reclaim
- 이후 public `mmap()`/`munmap()`을 추가할 수 있는 기반

## 2. 현재 기준선

현재 구현은 다음 특성을 가집니다.

- `isr_impl_page_fault()`는 모든 `#PF`를 panic으로 처리합니다.
- `struct vm_area`는 의도된 page table flags만 저장합니다.
- ELF segment와 user stack은 load 시점에 물리 페이지를 모두 할당합니다.
- `fork()`는 parent의 present page를 전부 새 페이지로 복사합니다.
- FAT regular file I/O는 `fat_inode_readall()`로 파일 전체를 `inode->buffer`에
  적재합니다.
- syscall의 read/write 경로는 user pointer를 VFS에 직접 전달합니다.
- `struct page`에 `refcount`가 있지만 공유 mapping과 page cache를 포괄하는
  수명 계약은 아직 없습니다.
- `xarray`는 page index 조회에 활용할 수 있지만 entry 제거와 cache 전체 순회에
  필요한 API가 부족합니다.

## 3. 핵심 불변식

### 3.1 Page 소유권

- buddy allocator는 refcount가 0인 미할당 page만 free list에 둡니다.
- page cache에 들어간 page는 address space가 소유 참조를 가집니다.
- page table mapping은 mapped page에 대한 참조를 가집니다.
- anonymous page와 page-cache page는 address space 파괴 시 서로 다른 경로로
  release합니다.
- PTE를 제거하기 전에 해당 mapping이 가진 page 참조를 식별할 수 있어야 합니다.
- reclaim은 dirty, loading 또는 mapped page를 무조건 해제하지 않습니다.

### 3.2 `irqlock`과 대기

- 이 저장소의 `irqlock`은 spinlock이나 공유 객체 락이 아닙니다.
- `irqlock_acquire()`는 현재 CPU의 IF 상태를 저장하고 interrupt를 비활성화하며,
  `irqlock_release()`는 저장한 IF 상태를 복원합니다.
- IF-disabled 구간에서 `task_wait()`로 전환하는 것은 현재 scheduler와 wait-list가
  의도적으로 지원하는 동작입니다.
- `completion_wait()`와 `mutex_lock()`도 조건 확인과 wait-list 등록 사이의
  lost wakeup을 막기 위해 이 패턴을 사용합니다.
- 따라서 `irqlock`이 활성화되었다는 사실만으로 대기를 금지하지 않습니다.
- 대기 가능 여부는 실제 공유 객체의 소유권, completion 경로와의 순환 의존성,
  hardware IRQ handler 여부를 기준으로 판단합니다.
- 동기 page fault는 현재 task에 귀속된 예외이므로 필요한 경우 I/O completion을
  기다릴 수 있습니다.

### 3.3 Fault 처리

- non-present fault와 protection fault를 구분합니다.
- reserved-bit fault는 복구 대상으로 취급하지 않습니다.
- fault 주소가 `vm_area` 밖이면 임의 페이지를 할당하지 않습니다.
- kernel 자체 주소 fault와 kernel이 user memory를 복사하다 발생한 fault를
  구분합니다.
- fault handler는 부분적으로 생성한 PTE/page/cache 상태를 실패 시 롤백합니다.
- 동일 cache page에 대한 동시 fault가 중복 I/O나 중복 page 생성을 유발하지
  않습니다.

### 3.4 File mapping

- file offset과 virtual address의 page offset 관계를 명시적으로 보존합니다.
- ELF의 file-backed 구간과 zero-fill 구간을 구분합니다.
- read-only file page는 여러 process가 공유할 수 있습니다.
- private writable mapping은 file page를 직접 수정하지 않고 write fault에서
  anonymous page로 분리합니다.
- truncate 이후 EOF 밖의 stale cache data를 노출하지 않습니다.
- buffered I/O와 mapped I/O는 동일한 cached page를 관찰합니다.

## 4. 제안 구조

세부 필드와 이름은 구현 중 조정할 수 있지만 역할 경계는 유지합니다.

### 4.1 Address space

regular inode는 page index에서 cached page를 찾는 address space를 가집니다.

address space의 책임:

- page index lookup/insert/remove
- cache page 생성 경쟁 직렬화
- filesystem별 page fill/writeback callback 호출
- dirty page 추적
- truncate/invalidate
- reclaim 후보 제공

filesystem callback의 책임:

- page index를 filesystem의 실제 저장 위치로 변환
- 부분 EOF page 처리
- read I/O와 writeback 수행
- filesystem metadata 변경과 오류 반환

### 4.2 Cache page 상태

최소 상태 모델:

- loading
- uptodate
- dirty
- writeback
- error
- reclaimable

동일 page를 요청한 task는 loading 중인 page를 새로 만들지 않고 기존 작업의
completion을 기다립니다.

### 4.3 VM area

`struct vm_area`는 최소한 다음 의도를 표현해야 합니다.

- anonymous 또는 file-backed mapping
- private/shared 정책
- page table 권한
- backing file 또는 address space 참조
- mapping 시작에 대응하는 file offset
- file-backed 길이
- 전체 memory 길이

ELF BSS와 demand stack은 같은 fault machinery를 사용하되 서로 다른 area 정책을
가질 수 있습니다.

## 5. 구현 단계

### 5.1 계약 및 테스트 기반

- page/PTE 소유권 표 작성
- page fault error-code helper 작성 및 hosted 테스트
- file offset/VA 변환 helper 테스트
- page cache 상태 전이 테스트 구성
- fault와 cache의 오류 주입 지점 정의

### 5.2 Page 수명과 xarray 보강

- `page_get()`/`page_put()` 추가
- allocator와 page refcount 불변식 연결
- PTE mapping이 보유하는 참조 규약 추가
- `xarray` entry 제거 및 필요한 순회 API 추가
- anonymous/cache page cleanup 경로 분리

### 5.3 Page fault와 usercopy

- `#PF` error code decode
- `vmtree` 기반 area lookup
- anonymous zero-fill demand fault
- invalid user fault의 task 종료 경로
- `copy_to_user()`/`copy_from_user()` 또는 동등한 usercopy 경계 추가
- syscall raw user pointer 직접 사용 제거

### 5.4 범용 page cache

- inode address space와 filesystem page callback 추가
- cache lookup, miss, fill, hit 경로 구현
- 동일 page 동시 load 직렬화
- cached page를 사용하는 positional read/write 구현
- dirty/invalidate 기본 경로 구현

### 5.5 Filesystem 연결

- CPIO regular file을 page cache read source로 연결
- FAT regular file의 `fat_inode_readall()` 의존 제거
- FAT cluster chain의 page 단위 read/write 구현
- buffered read/write를 공통 page cache로 전환
- truncate tail zeroing 및 cache invalidation 구현
- directory metadata cache와 regular data cache의 범위 구분

### 5.6 Demand-paged ELF

- `read_file_all()` 기반 exec 제거
- ELF header/program header positional read
- ELF segment별 file-backed `vm_area` 생성
- 최초 접근 시 page cache에서 page를 얻어 PTE 생성
- page 끝의 file/zero-fill 혼합 처리
- user stack demand allocation
- exec scratch image 원자성 유지

### 5.7 Fork와 COW

- 아직 fault 나지 않은 area를 lazy 상태로 복제
- read-only file page PFN 공유
- writable private PTE를 read-only COW 상태로 전환
- write protection fault에서 anonymous copy 생성
- parent/child TLB invalidation
- fork 실패 시 partial VMA/PTE/page 참조 롤백

### 5.8 Writeback과 reclaim

- dirty page writeback
- clean unmapped page reclaim
- allocator 압력 시 제한적 cache scan
- loading/writeback page 회수 방지
- writeback 오류의 dirty/error 상태 보존

### 5.9 Public mapping API

- 내부 demand mapping 계약이 안정화된 뒤 `mmap()`/`munmap()` syscall 추가
- 첫 공개 범위는 anonymous mapping과 file-backed private mapping을 우선
- shared writable mapping의 writeback/동기화 계약은 별도 단계로 검토

## 6. 검증 기준

### 6.1 기능

- 접근하지 않은 ELF page는 물리 page를 소비하지 않습니다.
- 최초 접근 시 정확한 file offset의 내용이 매핑됩니다.
- ELF BSS와 anonymous page는 0으로 초기화됩니다.
- 두 process의 read-only file mapping은 동일 cache page를 공유합니다.
- fork 이후 private write 시에만 child/parent page가 분리됩니다.
- 반복 buffered read는 cache hit 시 추가 disk request를 만들지 않습니다.
- dirty page writeback 후 다시 읽었을 때 변경 내용이 유지됩니다.
- truncate 이후 EOF 밖의 cache data가 보이지 않습니다.
- invalid user address는 kernel panic 없이 해당 실행 흐름에서 오류 처리됩니다.
- kernel page fault와 reserved-bit fault는 충분한 진단 정보와 함께 실패합니다.

### 6.2 빌드/테스트

각 단계에서 관련 범위에 따라 아래 경로를 순차 실행합니다.

```bash
make CONFIG=debug PLATFORM=pc-x64 NO_ANALYZER=1 >/dev/null
ASAN_OPTIONS=detect_leaks=0 make test CONFIG=debug PLATFORM=pc-x64 >/dev/null
make build UNIT_TEST=1 CONFIG=debug PLATFORM=pc-x64 NO_ANALYZER=1 >/dev/null
make unit-test QEMU_DISPNONE=1 CONFIG=debug PLATFORM=pc-x64 NO_ANALYZER=1
make CONFIG=release PLATFORM=pc-x64 >/dev/null
```

QEMU 유닛테스트는 `==== unit test end ====` 결과를 확인한 뒤 prompt에서 수동
종료합니다. 여러 `make` invocation은 동시에 실행하지 않습니다.

## 7. 주요 위험

- page cache 소유 page를 process teardown이 직접 free하는 double-free
- PTE ref와 cache ref 불균형으로 인한 영구 pin 또는 use-after-free
- cache loading/error 상태에서 lost wakeup
- page fault 중 재귀 allocation/fault
- COW 전환 시 parent TLB 미갱신
- ELF segment의 비정렬 file tail과 BSS 혼합 처리 오류
- FAT truncate/write 실패 후 cache와 on-disk metadata 불일치
- writeback completion 경로와 cache mutex 사이의 순환 의존성
- kernel raw user-pointer 접근을 일반 kernel fault로 오판

## 8. 문서화

구현 중 이 계획 문서를 진행 상태에 맞게 갱신합니다. 계약이 확정된 부분은 다음
정식 문서에 반영합니다.

- `docs/opal/mm/pagetable.md`
- `docs/opal/task/process.md`
- `docs/opal/task/fork-exec.md`
- `docs/opal/fs/vfs.md`
- `docs/opal/fs/cpio.md`
- `docs/opal/fs/fat.md`
- `docs/opal/syscall.md`
- `docs/testing.md`

새로 확인된 구조적 결함이나 의도적으로 남긴 제한은 재현 형태와 원인을 포함해
`docs/todo.md`에 기록합니다.
