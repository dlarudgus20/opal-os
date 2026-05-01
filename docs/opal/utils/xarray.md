# Xarray

## 개요
- 헤더: `kernel/include/opal/utils/xarray.h`
- 구현: `kernel/src/utils/xarray.c`
- 목적:
  - 희소 인덱스 -> 포인터 값 매핑
  - 단일 값 설정(`xarray_set`)과 연속 구간 설정(`xarray_set_range`) 지원

## 구조
- fanout:
  - 노드당 `XARRAY_SLOT_SIZE = 64` 슬롯
- 압축 표현:
  - 슬롯 값 하위 비트 플래그를 사용해
    - 직접 값(leaf value) 또는
    - 하위 노드 포인터
  - 를 구분합니다.
- `stride`:
  - range 설정 시 인덱스 증가에 따라 값 증가폭으로 사용됩니다.

## API
- `xarray_init(xa, stride)`
  - precondition: `stride > 0`이고 플래그 비트와 겹치지 않아야 함
- `xarray_destroy(xa)`
  - 동적으로 할당된 하위 노드를 모두 해제하고 루트를 초기화
- `xarray_get(xa, index)`
  - 값이 없으면 `NULL`
- `xarray_set(xa, index, value)`
  - 단일 인덱스 값을 설정
- `xarray_set_range(xa, index, len, start)`
  - `[index, index + len)` 범위를 `start + n * stride` 형태로 설정

## 주의사항
- 현재 구현은 포인터/값 인코딩을 위해 하위 비트 플래그를 사용합니다.
- `xarray_set_range()`는 오버플로우/범위 검사를 수행하며 실패 시 `false`를 반환합니다.
