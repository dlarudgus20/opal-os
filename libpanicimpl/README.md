# libpanicimpl

`libpanicimpl`은 테스트 환경에서 `panic_format` 구현을 제공하는 shared 라이브러리입니다.

## 역할
- `kc/assert.h`가 요구하는 `panic_format` 심볼 제공
- 테스트 중 assertion/panic 발생 시 메시지 출력 후 종료

## 빌드
```bash
make -C libpanicimpl build CONFIG=debug PLATFORM=pc-x64
```

산출물:
- `build/pc-x64/<config>/libpanicimpl.so`

## 테스트에서의 위치
- `kernel`, `libkc`, `libcoll` 등의 호스트 테스트 링크 시 `TEST_SHARED_REFS`로 참조됩니다.
