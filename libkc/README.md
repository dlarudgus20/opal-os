# libkc

`libkc`는 커널용 유사 libc 및 유틸리티 라이브러리입니다.

## 기능
- `mem*`, `str*` 계열 함수
- `sort`
- 포맷 엔진(`fmt`) + `ksnprintf`/`kvsnprintf`
- kassert/panic 매크로

## 문서
- 개요/동작 문서: [`docs/libkc/README.md`](../docs/libkc/README.md)

## 빌드
```bash
make -C libkc build CONFIG=debug PLATFORM=pc-x64
```
