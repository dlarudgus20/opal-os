# libuc

`libuc`는 사용자 프로그램이 링크하는 C 런타임 라이브러리다.
커널 syscall wrapper와 커널 ABI 상수는 `libopalsys`가 제공한다.

## 구성
- `_start`: 사용자 프로그램 진입점
- 간단한 I/O helper: `putchar`, `getchar`, `puts`, `getline`, `printf`
- panic 출력: `_panic_format`은 메시지를 stdout에 출력한 뒤 `task_exit()`한다

## libopalsys와의 관계
- `libuc`의 I/O helper는 `libopalsys`의 `read`, `write`, `task_exit` 같은 syscall wrapper 위에서 동작한다.
- 사용자 프로그램은 보통 `libuc`, `libopalsys`, `libkc`, `libkubsan`을 함께 링크한다.
- `FD_INVALID`, `open`, `stat`, `struct dirent`, HID/fbcon ABI 상수는 `libopalsys` 헤더를 포함해 사용한다.

## 제한
- 현재 커널은 사용자 포인터 범위 검증과 copy-in/copy-out을 완성하지 않았다. 잘못된 포인터는 커널 fault로 이어질 수 있다.
