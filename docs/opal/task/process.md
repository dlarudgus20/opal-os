# Process / User Task

## 개요
- 구현:
  - `kernel/src/task/process.c`
  - `kernel/include/opal/task/process.h`
- 목적:
  - 프로세스 객체 수명 관리
  - 유저 ELF 로딩 및 유저 태스크 시작
  - 프로세스 단위 FD 테이블 관리

## `struct process`
- `id`, `pid_node`:
  - PID 트리(`rbtree`)에 연결되는 식별자/노드
- `refcount`:
  - 프로세스 수명 참조 카운트
- `task_list`:
  - 프로세스 소속 태스크 연결 리스트
- `vmtree`:
  - 유저 가상주소 구간 메타데이터(non-hole만 저장)
- `pagetable`:
  - 프로세스 페이지테이블
- `open_files`:
  - FD -> `struct file *` 동적 배열(`dynarray`)

## 수명 관리
- `process_create()`:
  - `pagetable_create()` + `kzalloc(process)` 후 PID 트리에 등록
- `process_retain()` / `process_release()`:
  - `irqlock` 구역에서 `refcount` 증감
  - 0이 되면 `process_free()`로 정리
- `process_free()`:
  - precondition: `task_list`가 비어 있어야 함
  - 열린 파일 참조 해제
  - PID 트리 제거
  - `vmtree` 기반 매핑 페이지 해제 + `pagetable_destroy()`

## 유저 ELF 로드
- 진입점: `process_load_elf(proc, elf, size, out_task)`
- 반환 타입: `kerrno_t`
- 검증:
  - ELF ident/type/machine/phdr 범위 검증
  - `PT_LOAD`만 허용(일부 타입 제외)
  - `p_vaddr/p_offset`는 페이지 정렬 요구
  - 유저 스택 하단(`0x0000400000000000`) 상한 검사
- 매핑:
  - `map_section()`이 `mm_alloc_page(order)` + `pagetable_map()` 수행
  - ELF `PF_W`에 따라 writable 플래그 반영
  - 실패 시 부분 매핑 롤백(`pagetable_unmap`, `vmtree_remove`)
- 유저 스택:
  - 현재 1페이지를 `ustack_bottom`에 매핑
- 시작:
  - `process_create_usertask()`로 태스크 생성 후 `enter_userland(entry, stack_top)`
- 주요 반환 코드:
  - `OPAL_OK`: 성공
  - `OPAL_ENOMEM`: 매핑/태스크 생성 중 메모리 부족
  - `OPAL_EBADIMAGE`: ELF 구조/헤더/프로그램 헤더 불일치 등 이미지 손상
  - `OPAL_ENOEXEC`: 실행 불가 형식/타겟 아키텍처 불일치/실행 정책 위반

## FD 테이블 API
- `process_open_file(proc, fd, file)`:
  - `fd == FD_INVALID`이면 빈 FD를 할당한다.
  - 지정 FD는 비어 있을 때만 등록한다.
  - 실패 시 `FD_INVALID`를 반환한다.
- `process_get_file(proc, fd)`:
  - 유효한 FD면 파일 참조 1 증가 후 반환, 아니면 `NULL`
- `process_close_file(proc, fd)`:
  - 슬롯 파일 참조 해제 후 `NULL`로 표시
- `process_dup_fd(proc, oldfd, newfd)`:
  - `oldfd`의 파일 참조를 복제한다.
  - `newfd == FD_INVALID`이면 빈 FD를 할당한다.
  - 지정 `newfd`는 기존 파일을 닫거나 교체하지 않는다.
  - 실패 시 `FD_INVALID`를 반환한다.

## FD 테이블 구조/정책
- `struct filetable`은 프로세스별 FD 번호를 `struct file *`로 매핑한다.
- 주요 상태:
  - `files`: FD 번호별 파일 포인터 배열
  - `bitmap`: 열린 슬롯 표시
  - `capacity`: 현재 배열 용량
  - `count`: 열린 FD 수
  - `end_fd`: 순회가 필요한 마지막 FD의 다음 번호
  - `next_fd`: 자동 할당 시작 힌트
  - `inline_files`, `inline_bitmap`: 작은 FD table용 내부 저장소
- `FD_INVALID`는 자동 할당 요청 또는 실패 반환값으로 사용한다.
- `FD_MAX`는 지정 가능한 최대 FD 번호다.
- 자동 할당(`filetable_insert`)은 `next_fd` 힌트부터 빈 슬롯을 찾고, 없으면 앞쪽도 순회한다.
- 지정 삽입(`filetable_insert_at`)은 FD가 범위 안이고 해당 슬롯이 비어 있을 때만 성공한다.
- 이미 열린 FD에 지정 삽입하면 기존 파일을 닫거나 교체하지 않고 `OPAL_EEXIST`로 실패한다.
- insert/dup/clone으로 새 슬롯에 파일을 넣으면 file reference를 retain한다.
- get은 열린 FD면 retain 후 반환하고, 닫힌 FD나 범위 밖 FD면 `NULL`을 반환한다.
- remove는 열린 FD만 제거하고 release한다.
- clone은 열린 FD만 같은 번호로 복제하고 `next_fd`를 원본과 동일하게 맞춘다.
