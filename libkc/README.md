# libkc

`libkc`는 커널용 유사 libc 및 유틸리티 라이브러리입니다.

## 기능
- `mem*`, `str*` 계열 함수
- `sort`
- 포맷 엔진(`fmt`) + `snprintf_s`/`vsnprintf_s`
- assert/panic 매크로

## 빌드
```bash
make -C libkc build CONFIG=debug PLATFORM=pc-x64
```

## 테스트
```bash
make -C libkc test CONFIG=debug PLATFORM=pc-x64
```

테스트 빌드에는 `TEST_DO_NOT_LINK=1` 설정이 켜져 있습니다.
테스트 실행 파일은 `dlopen`으로 libkc.so를 열어 사용합니다.

## 테스트 링크 주의 (`--exclude-libs=libkc`)
`libkc`는 libc 유사 심볼을 포함하므로 hosted 테스트 시 심볼 충돌을 막기 위해 아래 플래그를 권장합니다.

```make
LDFLAGS_ON_TEST := -Wl,--exclude-libs=libkc
```
