# Page Table

## 개요
- 헤더: `kernel/platform/<platform>/include/opal/platform/mm/pagetable.h`
- 구현: `kernel/platform/<platform>/src/mm/pagetable.c`
- 역할:
  - 가상 메모리 매핑/해제
  - 주소 조회
  - 프로세스 페이지테이블 생성/파기/적용

## 주요 API
- `pagetable_create()`:
  - 커널 기본 페이지테이블을 clone한 사용자 페이지테이블 생성
- `pagetable_destroy(ptbl)`:
  - 전체 유저 공간 언매핑 후 루트 페이지 반환
- `pagetable_apply(ptbl)`:
  - cpu의 pagetable pointer 교체 (예: x86 `cr3`)
- `pagetable_map(ptbl, va, pa, len, flags)`:
  - 구간 매핑(조건 만족 시 huge 사용)
  - 기존 매핑 overlap은 실패
  - 실패 시 내부적으로 부분 롤백 수행
  - 반환값:
    - 성공: 매핑 종료 가상주소(일반적으로 `va + len`)
    - 실패: `0` (이미 매핑된 구간/중간 할당 실패 등)
    - `len == 0`이면 `va`를 반환
- `pagetable_unmap(ptbl, va, len, flush_tlb)`:
  - 구간 매핑 해제
  - 필요 시 상위 테이블도 정리
  - `flush_tlb`가 true면 cpu tlb flush 수행
    - 구간 크기에 따라 개별 페이지 flush / 전체 flush 선택해서 수행
  - huge page를 부분 해제해야 할 때 하위 페이지로 분해 후 수행
    - 이때 page allocation 실패 가능.
  - 반환값:
    - 완료: 해제 종료 가상주소(일반적으로 `va + len`)
    - 중단: 실제로 해제를 진행한 마지막 지점 주소
      - 예: huge page 분해에 필요한 페이지 할당 실패 시
    - `len == 0`이면 `va`를 반환
- `pagetable_lookup(ptbl, va, &pa_out)`:
  - 매핑된 물리 주소 조회
  - 반환값:
    - `true`: 조회 성공, `pa_out`에 `va` offset이 반영된 물리 주소 반환
    - `false`: `ptbl == NULL`, `pa_out == NULL`, 또는 미매핑 주소
  - 1GiB/2MiB huge page와 4KiB page 모두 leaf 기준 물리 주소에 page offset을 더해 반환한다.
