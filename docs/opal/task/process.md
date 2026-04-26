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
  - `task_list`가 비어 있어야 함(assert)
  - 열린 파일 참조 해제
  - PID 트리 제거
  - `vmtree` 기반 매핑 페이지 해제 + `pagetable_destroy()`

## 유저 ELF 로드
- 진입점: `process_load_elf(proc, elf, size, out_task)`
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

## FD 테이블 API
- `process_open_file(proc, file)`:
  - FD 슬롯 추가, 파일 참조 1 증가
- `process_get_file(proc, fd)`:
  - 유효한 FD면 파일 참조 1 증가 후 반환, 아니면 `NULL`
- `process_close_file(proc, fd)`:
  - 슬롯 파일 참조 해제 후 `NULL`로 표시
