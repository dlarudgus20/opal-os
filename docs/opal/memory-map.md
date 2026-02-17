# Memory Map (pc-x64)

## 1. 메모리 맵

```text
virtual range                                physical range                    note
--------------------------------------------|---------------------------------|----------------------------
[0x00000000 00000000 ~ 0x00000000 00600000)  [0x00000000 ~ 0x00600000)         identity map
[0xffff8000 00000000 ~      <dynamic>     )          <dynamic>                 page table
[0xffff9000 00000000 ~      <dynamic>     )          <dynamic>                 struct page
[0xffffffff 80000000 ~ 0xffffffff 80800000)  [0x00200000 ~ 0x00a00000)         higher-half bootstrap window
[0xffffffff 8f000000 ~ 0xffffffff 8f200000)  [__stack_bottom_lba ~ +0x200000)  kernel stack mapping
```

## 2. 부트 코드의 임시 페이지 테이블
[`boot.asm`](../../kernel/platform/pc-x64/src/boot/boot.asm)의 `.bss`에 임시 테이블 버퍼를 확보합니다.

- `tmp_table: resb 0x5000`
- 사용 구조:
  - `+0x0000`: PML4
  - `+0x1000`: PDPT_low
  - `+0x2000`: PDPT_high
  - `+0x3000`: PDT_low
  - `+0x4000`: PDT_high

총 5페이지(20 KiB)입니다.

### 2.1 low 영역 (identity 매핑)
`PDT_low`:
- `0x00000000 -> 0x00000000` (2 MiB)
- `0x00200000 -> 0x00200000` (2 MiB)
- `0x00400000 -> 0x00400000` (2 MiB)

`0~6 MiB` 구간을 identity로 열어 둡니다.

### 2.2 higher-half 커널 창
`PDT_high`의 일부 엔트리:
- `0xffffffff80000000 -> 0x00200000`
- `0xffffffff80200000 -> 0x00400000`
- `0xffffffff80400000 -> 0x00600000`
- `0xffffffff80600000 -> 0x00800000`

higher-half 시작점에서 `2~10 MiB` 물리 구간을 열어 둡니다.

### 2.3 higher-half 스택 창
`PDT_high`의 `0x3c0` 바이트 오프셋(120번 엔트리)에:
- `__stack_bottom_lba + 0x83`

를 설정해 2 MiB 스택 페이지를 매핑합니다.

연결되는 스택 관련 심볼:
- `__stack_bottom_lba`
- `__stack_top_lba` (`.bss`에서 2 MiB 크기로 예약)
- 부트 후 64비트 진입에서 `rsp = 0xffffffff8f200000`
