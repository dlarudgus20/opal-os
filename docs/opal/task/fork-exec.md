# fork/exec 구현 설명

이 문서는 현재 C 커널 브랜치에 추가된 `process_fork()`와
`process_exec_elf_file()` 구현의 구조와 의도를 정리한다.

## 목표

이번 구현의 핵심 목표는 두 가지다.

1. `fork`
   - 현재 프로세스의 fd table을 복제한다.
   - 현재 프로세스의 전체 user vmem을 eager copy한다.
   - 현재 task 하나에 대응하는 child task를 만든다.
   - 단, child가 `fork()`에서 user mode로 복귀하는 trampoline은 아직 후속 작업으로 둔다.

2. `exec`
   - fd table은 유지한다.
   - 프로세스의 user address space를 새 ELF image로 교체한다.
   - 실패 시 기존 image를 보존한다.
   - single-thread process에서만 성공한다.

## VM 영역 표현

기존에는 `vmtree`의 non-hole entry에 `proc` 포인터를 넣었다. 이제는 `struct vm_area *`를 넣는다.

```c
struct vm_area {
    page_entry_t ptbl_flags;
};
```

현재 `vm_area`가 저장하는 정보는 page table mapping flag뿐이다. 그래도 이 구조를 둔 이유는
PTE에서 직접 flags를 역조회하지 않고, VM 영역의 의도된 권한을 별도로 보존하기 위해서다.

PTE leaf entry에는 accessed/dirty 같은 하드웨어 상태가 섞일 수 있다. fork가 복제해야 하는 것은
그 순간의 하드웨어 상태가 아니라, 해당 virtual range가 어떤 권한으로 매핑되어야 하는지에 대한
VM 계약이다. 따라서 fork는 parent PTE에서 PA만 조회하고, mapping flags는 parent `vm_area`에서
가져온다.

현재 규약은 다음과 같다.

- `vmtree.entry == NULL`: hole
- `vmtree.entry != NULL`: `struct vm_area *`
- `vm_area->ptbl_flags`: `PTBL_USER`, `PTBL_WRITABLE` 같은 kernel 추상 page table flags

## 주소공간 정리

주소공간 정리는 세 단계를 함께 수행해야 한다.

1. vmtree range를 순회하면서 각 mapped physical page를 free한다.
2. 각 range의 `struct vm_area`를 free한다.
3. pagetable과 vmtree 자체를 destroy한다.

이를 위해 process 쪽에 private helper들이 추가됐다.

- `free_mapped_pages_all(ptbl, vmtree)`
- `free_vm_areas_all(vmtree)`
- `process_image_destroy_parts(ptbl, vmtree)`
- `process_image_destroy(image)`

`process_free()`도 이제 이 공통 정리 경로를 사용한다. 이 변경 때문에 vmtree entry lifetime이
명확해졌다. non-hole entry는 process address space destroy 시 반드시 해제된다.

## vmtree_move()

`exec`는 새 address space를 scratch image에 먼저 만든 뒤 기존 process에 commit해야 한다.
이때 `struct vmtree`를 단순 대입하면 안 된다. `vmtree.root`는 tree 내부의 embedded
`root_node` 주소를 태그한 값을 들고 있기 때문이다.

그래서 `vmtree_move(dst, src)`를 추가했다.

동작은 다음과 같다.

- `src->root_node`를 `dst->root_node`로 복사한다.
- `dst->root`는 `dst->root_node` 주소 기준으로 다시 태그한다.
- `src`는 빈 tree로 다시 초기화한다.

이 API는 exec commit처럼 vmtree ownership을 옮겨야 하는 경우에만 사용한다.

## filetable_clone()

fork는 parent의 open file table을 child에 같은 fd 번호로 복제해야 한다.

`filetable_clone(dst, src)`는 다음 방식으로 동작한다.

- `dst`를 빈 table로 초기화한다.
- `src[0, end_fd)`를 순회한다.
- 열린 fd만 `filetable_insert_at(dst, fd, file)`로 삽입한다.
- `filetable_insert_at()`이 내부에서 `file_retain()`하므로 clone 쪽에서 별도 retain은 하지 않는다.
- 중간 실패 시 partial clone을 `filetable_destroy()`로 정리하고 다시 init한다.
- clone 성공 후 `next_fd`를 src와 동일하게 맞춘다.

이 helper는 `filetable` 내부 구조를 외부로 노출하지 않기 위해 `filetable.c` 내부에서 구현한다.

## fork 흐름

`process_fork(frame, proc_out, task_out)`의 현재 흐름은 다음과 같다.

1. parent는 `process_current()`로 얻는다.
2. `process_create()`로 child process를 만든다.
3. child fd table을 `filetable_clone()`으로 복제한다.
4. parent vmtree를 순회하며 child vmem을 복제한다.
5. child task placeholder를 만든다.
6. child process와 child task를 out parameter로 반환한다.

### vmem 복제

vmem 복제는 `process_clone_vmem()`과 `clone_vmem_range()`가 담당한다.

각 parent vmtree entry마다 다음을 수행한다.

- parent `vm_area`를 복사해 child `vm_area`를 새로 할당한다.
- child vmtree에 같은 `[start, end)` range를 삽입한다.
- range를 `PAGE_SIZE` 단위로 순회한다.
- parent pagetable에서 VA에 대응하는 PA를 `pagetable_lookup()`으로 얻는다.
- 새 physical page를 할당한다.
- direct map을 통해 page 내용을 복사한다.
- child pagetable에 같은 VA로 매핑한다.
- mapping flags는 child `vm_area->ptbl_flags`를 사용한다.

현재 fork는 COW가 아니라 eager copy다.

### fork 실패 처리

fd clone 또는 vmem clone 중 실패하면 child process를 release한다. child release는 process destroy
경로를 통해 이미 복제된 file ref, physical pages, vm_area, pagetable, vmtree를 정리한다.

range 복제 도중 실패한 경우에는 해당 range에서 이미 매핑된 page만 먼저 정리하고, child vmtree에서
range를 제거한 뒤 child `vm_area`를 free한다.

### child task placeholder

아직 syscall frame에서 child가 user mode로 복귀하는 trampoline은 구현하지 않았다.

그래서 child task는 현재 `fork_child_stub()` entry로 생성만 하고 resume하지 않는다. stub는
실행되면 panic하도록 되어 있다. syscall `SYS_FORK`는 parent에게 child pid를 돌려주지만,
child는 아직 runnable 상태가 아니다.

후속 trampoline 작업에서는 다음이 필요하다.

- `isr_stackframe`을 child kernel stack에 복사한다.
- child frame의 return value를 `0`으로 설정한다.
- child context가 그 frame으로 `iretq` 복귀하도록 platform task helper를 추가한다.
- 그 시점에 child task를 resume한다.

## exec 흐름

`process_exec_elf_file(proc, file, task_out)`는 fd table을 제외한 process image를 새 ELF로
교체한다.

현재 exec는 single-thread process에서만 성공한다. `proc->task_list`에 current task 하나만
있지 않으면 `OPAL_EBUSY`를 반환한다.

성공 흐름은 다음과 같다.

1. `read_file_all()`로 ELF 파일 전체를 kernel buffer에 읽는다.
2. `process_image_create()`로 scratch pagetable/vmtree를 만든다.
3. `load_elf_image()`로 scratch image에 ELF segment와 user stack을 매핑한다.
4. 새 user task를 suspended 상태로 만든다.
5. old pagetable과 old vmtree를 따로 보관한다.
6. scratch pagetable/vmtree를 process에 commit한다.
7. current process라면 새 pagetable을 `pagetable_apply()`로 적용한다.
8. old image를 destroy한다.
9. 새 task를 out parameter로 반환한다.

syscall `SYS_EXEC` 경로는 반환된 task를 resume한 뒤 현재 old task를 `task_exit()`로 종료한다.
따라서 exec 성공 시 user caller로는 반환하지 않는다.

## exec 원자성

exec는 기존 image를 바로 지우지 않는다. 먼저 scratch image에 ELF load를 완료한 뒤 commit한다.

따라서 다음 실패는 기존 process image를 보존한다.

- 파일 길이 조회 실패
- 파일 read 실패
- ELF validation 실패
- segment mapping 실패
- user stack mapping 실패
- 새 task 생성 실패

commit 이후에는 새 pagetable/vmtree가 process에 들어간 상태이므로 old image를 정리한다.

## ELF loader 분리

기존 `process_load_elf()`는 ELF validation, segment mapping, user task 생성을 한 함수에서
수행했다. exec의 scratch image load를 위해 내부 로직을 분리했다.

- `load_elf_image(ptbl, vmtree, elf, size, entry_out, stack_out, stack_size_out)`
  - ELF를 검증한다.
  - loadable segment를 주어진 pagetable/vmtree에 매핑한다.
  - user stack을 매핑한다.
  - entry와 stack 정보를 반환한다.

- `process_load_elf(proc, elf, size, out)`
  - 기존 public API를 유지한다.
  - process의 현재 pagetable/vmtree에 image를 로드한다.
  - user task를 만들고 resume한다.

- `process_exec_elf_file(proc, file, task_out)`
  - scratch image에 `load_elf_image()`를 호출한다.
  - 새 task는 suspended로 만든 뒤 syscall 경로에서 resume한다.

## syscall 연결

`SYS_FORK`와 `SYS_EXEC`가 kernel syscall dispatch에 연결됐다.

`SYS_FORK`:

- platform int80 handler에서 받은 `isr_stackframe *`를 dispatch에 전달한다.
- `process_fork()`를 호출한다.
- 성공 시 parent return value는 child pid다.
- 현재 child return `0`은 trampoline 미구현으로 아직 연결되지 않았다.

`SYS_EXEC`:

- fd를 검증한다.
- current process fd table에서 file을 얻는다.
- `process_exec_elf_file(process_current(), file, &task)`를 호출한다.
- 성공하면 새 task를 resume하고 현재 task를 종료한다.
- 실패하면 음수 errno를 반환한다.

libuc에는 다음 wrapper가 추가됐다.

- `pid_t fork(void)`
- `int exec(int fd)`

## 현재 제한

현재 구현은 fork의 address space와 fd table 복제 기반을 마련한 상태다. 하지만 child가 실제로
`fork()` syscall return 지점으로 복귀하지는 않는다.

남은 주요 작업은 다음과 같다.

- x86_64 child fork trampoline 구현
- child `isr_stackframe` 복사 및 return register 조정
- child task resume 시점 연결
- fork runtime 검증

exec는 single-thread 조건에서 동작하도록 설계했다. multi-thread process에서 exec를 호출하면
`OPAL_EBUSY`로 실패한다.

## 검증 상태

이번 구현 후 확인한 명령은 다음과 같다.

```sh
make -C kernel build CONFIG=debug PLATFORM=pc-x64
make -C libuc build CONFIG=debug PLATFORM=pc-x64
make -C uinit build CONFIG=debug PLATFORM=pc-x64
ASAN_OPTIONS=detect_leaks=0 make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

결과:

- kernel debug build 성공
- libuc debug build 성공
- uinit debug build/link 성공
- kernel hosted test 27개 통과

