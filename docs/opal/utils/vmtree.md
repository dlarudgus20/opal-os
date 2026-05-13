# Vmtree

## 개요
- 헤더: `kernel/include/opal/utils/vmtree.h`
- 구현: `kernel/src/utils/vmtree.c`
- 목적:
  - 가상 주소 구간을 hole/non-hole로 관리하는 B-Tree 기반 구조
  - 프로세스 유저 매핑 메타데이터 저장(`process.vmtree`)

## 모델
- 기본 상태는 전체 주소공간이 hole(`entry = NULL`)입니다.
- non-hole 구간만 의미 있는 매핑으로 취급합니다.
- 인접 구간이 같은 `entry`를 가지면 자동 병합합니다.

## 내부 표현
- 노드:
  - `pivots[VMTREE_NODE_SLOTS - 1]`
  - `slots[VMTREE_NODE_SLOTS]`
- `pivots`의 미사용 슬롯은 모두 `UINTPTR_MAX`로 채웁니다.
- 노드 full 판정은 `pivots[VMTREE_NODE_SLOTS - 2] != UINTPTR_MAX`입니다.
- `slots`는 `(pointer | flags)` 형식 태그 값입니다.
  - leaf면 `slots`가 직접 `entry` 값을 담습니다.
  - 내부 노드면 `slots`가 자식 노드 포인터를 담습니다.
- 루트도 태그 포인터를 사용하며 `vmtree.root = (&root_node | flags)` 형태입니다.

## API
- `vmtree_init(tree)` / `vmtree_destroy(tree)`
- `vmtree_get(tree, addr)`
  - `addr`를 포함하는 `[start, end)` 구간과 `entry`를 반환
- `vmtree_insert(tree, start, end, entry)`
  - hole 구간에만 삽입 가능, 겹치면 `VMTREE_ERR_EXISTS`
- `vmtree_set(tree, start, end, entry)`
  - 기존 non-hole 구간에만 덮어쓰기 가능, hole이 섞이면 `VMTREE_ERR_NOENT`
- `vmtree_remove(tree, start, end)`
  - 구간을 hole로 설정

## 반복자
- 시작: `struct vmtree_iter it = vmtree_before_begin(&tree);`
- 진행: `vmtree_iter_next(&it, &entry)`
- 반환값:
  - `true`: 다음 non-hole 구간을 `entry`에 반환
  - `false`: 반복 종료 또는 잘못된 인자, `entry`는 유효하지 않음
- 동작:
  - hole 구간은 건너뛰고 non-hole 구간만 오름차순 순회
