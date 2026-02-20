# libpanicimpl

`libpanicimpl`은 hosted 테스트에서 libkc가 요구하는 `panic_format` 함수를 제공하는 shared 라이브러리입니다.

## 기능
- [`kc/assert.h`](../libkc/include/kc/assert.h)가 요구하는 `panic_format` 함수 제공
- assert/panic 발생 시 메시지 출력 후 종료

## 빌드
```bash
make -C libpanicimpl build CONFIG=debug PLATFORM=pc-x64
```

## 사용법
```makefile
TEST_SHARED_REFS := libpanicimpl
```
