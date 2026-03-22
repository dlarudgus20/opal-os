# Shell

## 개요
- 구현 위치: `kernel/src/shell/`
- 외부 명령 헤더:
  - 공통: `kernel/include/opal/shell/shell_cmd.h`
  - 플랫폼: `kernel/platform/pc-x64/include/opal/platform/shell/shell_cmd.h`
- 공통 유틸 헤더: `kernel/include/opal/shell/utils.h`

## 실행 흐름
- 진입점: `shell_start()`가 `run_shell()` 태스크를 생성
- `run_shell()`:
  - 배너 출력
  - 프롬프트(`root@opal:~$`) 표시
  - `tty0_getline()`으로 한 줄 입력
  - `handle_command()`로 명령 실행
- `exit` 동작:
  - `cmd_exit()`가 `g_exit=true`를 설정
  - 현재 구현은 셸 태스크를 끝내지 않고 바깥 루프로 재진입해 셸을 다시 시작

## 명령 디스패치 구조
- 명령 목록은 `g_commands[]` 정적 테이블로 관리
- 각 항목은 아래 정보를 가진다:
  - 명령 이름
  - `help` 출력용 짧은 설명
  - 핸들러 종류(`argv` 파싱 필요 여부)
  - 함수 포인터(`args_handler` 또는 `argv_handler`)
- 검색:
  - 입력 첫 토큰 길이(`cmd_len`)를 계산
  - `find_command()`에서 이름 정확 일치로 테이블 조회

## 인자 처리 모델
- `CMD_ARGV`:
  - `parse_argv()`를 통해 `argc/argv` 생성 후 핸들러 호출
  - quote/escape를 처리하는 명령에 사용
- `CMD_ARGS`:
  - 명령명 뒤 raw 문자열을 그대로 핸들러에 전달
  - 현재 `echo`, `klog`에 사용

## `parse_argv()` 규칙
- 공백은 토큰 구분자로 사용
- 작은따옴표/큰따옴표로 묶인 구간은 하나의 토큰으로 취급
- `\` escape 지원
- 오류 코드:
  - `SHELL_PARSE_TOO_MANY_ARGS`
  - `SHELL_PARSE_UNTERMINATED_QUOTE`
  - `SHELL_PARSE_TRAILING_ESCAPE`

## 명령 리턴 코드 규약
- 성공: `0`
- 실패: `1`
- shell core는 반환값을 받아 상태 메시지를 자동 출력하지 않으며, 각 명령이 필요한 오류 메시지를 직접 출력한다.

## 공용 유틸
### `shell_hexdump()`
- 선언: `void shell_hexdump(const unsigned char *ptr, size_t len);`
- 구현: `kernel/src/shell/hexdump.c`
- 동작:
  - 16바이트 단위 hex + ASCII 라인 출력
  - 256바이트 출력마다 일시 정지
  - `quit`/`q` 입력 시 중단, 엔터 입력 시 다음 페이지 진행
