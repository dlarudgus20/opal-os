# TODO

## current

- pata pio
- fs
- apic

## archived

### 리소스 내장 방식 개선 (`.c` 대체)
- 목표: 폰트/리소스 내장 방식을 `.c` 생성 중심에서 바이너리 리소스 중심으로 정리
- 후보 방식:
  - `#embed` 직접 사용
  - `objcopy` 또는 `ld -b binary`로 바이너리 섹션 연결

### 화면 출력 속도 개선
- 목표: framebuffer/TTY 출력 경로의 체감 지연 감소
- 우선 점검 대상:
  - `fb_draw_char`
  - 문자열 출력 경로의 반복 호출/분기 비용
  - 스크롤 시 메모리 이동 비용
- 개선 후보:
  - glyph 캐시/사전 전개
  - `memcpy` 기반 blit 최적화
  - dirty region 갱신
  - 문자열 단위 일괄 출력
