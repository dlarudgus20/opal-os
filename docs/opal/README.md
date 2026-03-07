# opal-os 내부 문서

- [`memory-map.md`](memory-map.md): 메모리 맵 요약
- [`buddy.md`](buddy.md): 버디 페이지 할당기
- [`slab.md`](slab.md): 슬랩 오브젝트 할당기
- [`task.md`](task.md): 태스크/스케줄러
- [`interrupt-io.md`](interrupt-io.md): 인터럽트/입력/시리얼 IO 경로
- framebuffer 콘솔/TTY 관련 구현은 `kernel/src/fb/fb.c`, `kernel/src/tty/fb_tty.c`를 참고하세요.
