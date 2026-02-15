# Higher-Half Paging (pc-x64)

이 문서는 현재 구현된 [`kernel/platform/pc-x64/src/boot/boot.asm`](../../kernel/platform/pc-x64/src/boot/boot.asm) 기준 higher-half paging 초기화 과정을 설명합니다.

## 0. 메모리 맵

```text
Physical Memory (boot-time)

[0x00000000 ~ 0x00200000)    low memory / reserved
[0x00200000 ~ 0x00800000)    kernel
[0x00800000 ~      -    )    remaining physical memory

Virtual Memory (current bootstrap mapping)

virtual range                                physical range                    note
--------------------------------------------|---------------------------------|----------------------------
[0x00000000 00000000 ~ 0x00000000 00600000)  [0x00000000 ~ 0x00600000)         identity map
[0xffffffff 80000000 ~ 0xffffffff 80800000)  [0x00200000 ~ 0x00a00000)         higher-half bootstrap window
[0xffffffff 8f000000 ~ 0xffffffff 8f200000)  [__stack_bottom_lba ~ +0x200000)  kernel stack mapping

Key constants
- higher-half base  : 0xffffffff80000000
- displacement      : 0xffffffff80000000 - 0x00200000
- temporary tables  : tmp_table (0x5000 bytes)
```

## 1. 목표
- 부트 직후 32비트 보호 모드에서 페이지 테이블을 임시 구성
- paging + long mode 활성화 후 64비트 higher-half 가상 주소에서 커널 실행
- 커널 코드/데이터와 스택을 higher-half 주소로 접근 가능하게 구성

## 2. 링크 배치와 주소 기준
링커 스크립트([`kernel/platform/pc-x64/src/linker.ld`](../../kernel/platform/pc-x64/src/linker.ld))는 다음 기준을 사용합니다.

- 물리 로드 시작: `0x00200000` (2 MiB)
- higher-half 시작: `0xffffffff80000000`
- 변위:
  - `__higher_half_displacement = 0xffffffff80000000 - 0x00200000`

즉 커널 섹션은 higher-half 가상 주소에 배치되지만, 실제 로드는 2 MiB 물리 주소부터 이뤄집니다 (`AT(...)` 사용).

## 3. 부트 코드의 임시 페이지 테이블
[`boot.asm`](../../kernel/platform/pc-x64/src/boot/boot.asm)의 `.bss`에 임시 테이블 버퍼를 확보합니다.

- `tmp_table: resb 0x5000`
- 사용 구조:
  - `+0x0000`: PML4
  - `+0x1000`: PDPT_low
  - `+0x2000`: PDPT_high
  - `+0x3000`: PDT_low
  - `+0x4000`: PDT_high

총 5페이지(20 KiB)이며 현재 코드에서 모두 사용합니다.

## 4. 엔트리 구성
초기화 순서는 다음과 같습니다.

1. `PML4[0] -> PDPT_low`
2. `PML4[511] -> PDPT_high`
   (`[ebx + 0xff8]`, 511번 엔트리)
3. `PDPT_low[0] -> PDT_low`
4. `PDPT_high[510] -> PDT_high`
   (`[ebx + 0xff0]`, 510번 엔트리)

그 뒤 PDT에 2 MiB large page(PS=1) 엔트리를 직접 설정합니다.

## 5. 실제 매핑 범위
### 5.1 low 영역 (identity 매핑)
`PDT_low`:
- `0x00000000 -> 0x00000000` (2 MiB)
- `0x00200000 -> 0x00200000` (2 MiB)
- `0x00400000 -> 0x00400000` (2 MiB)

즉, 대략 `0~6 MiB` 구간을 identity로 열어 둡니다.

### 5.2 higher-half 커널 창
`PDT_high`의 일부 엔트리:
- `0xffffffff80000000 -> 0x00200000`
- `0xffffffff80200000 -> 0x00400000`
- `0xffffffff80400000 -> 0x00600000`
- `0xffffffff80600000 -> 0x00800000`

즉 현재 구현은 higher-half 시작점에서 최소 `2~10 MiB` 물리 구간을 2 MiB 페이지 단위로 연결합니다.

### 5.3 higher-half 스택 창
`PDT_high`의 `0x3c0` 바이트 오프셋(120번 엔트리)에:
- `__stack_bottom_lba + 0x83`

를 설정해 2 MiB 스택 페이지를 매핑합니다.

연결되는 스택 관련 심볼:
- `__stack_bottom_lba`
- `__stack_top_lba` (`.bss`에서 2 MiB 크기로 예약)
- 부트 후 64비트 진입에서 `rsp = 0xffffffff8f200000`

## 6. 전환 절차
[`boot.asm`](../../kernel/platform/pc-x64/src/boot/boot.asm) 기준:

1. `CR4.PAE = 1`
2. `EFER.LME = 1` (MSR `0xC0000080`)
3. `CR3 = tmp_table - DISPLACEMENT` (PML4 물리 주소)
4. `CR0.PG = 1`
5. `lgdt` 후 far jump로 64비트 코드 세그먼트 진입
6. 세그먼트 레지스터 정리 후 `kmain_platform` 호출

## 7. 설계 의도와 현재 제약
의도:
- 부트 경로를 단순하게 유지하면서 higher-half 실행을 빠르게 시작
- 4KiB PT 단계 없이 2 MiB 페이지로 초기 맵을 단순화

제약:
- 초기 매핑 범위가 고정 폭(엔트리 하드코딩)
- 물리 메모리 전체 맵핑/동적 매핑은 아직 미구현
- 이후 VM 서브시스템이 확장되면 이 초기 맵은 bootstrap 단계로 한정될 가능성이 큼

## 8. 관련 파일
- [`kernel/platform/pc-x64/src/boot/boot.asm`](../../kernel/platform/pc-x64/src/boot/boot.asm)
- [`kernel/platform/pc-x64/src/linker.ld`](../../kernel/platform/pc-x64/src/linker.ld)
- [`kernel/platform/pc-x64/src/boot/boot.c`](../../kernel/platform/pc-x64/src/boot/boot.c)
- [`kernel/src/kmain.c`](../../kernel/src/kmain.c)
