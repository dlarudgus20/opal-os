# libkubsan

`libkubsan`은 커널에 UBSAN 핸들러를 제공하는 정적 라이브러리입니다.

## 기능
- `-fsanitize=undefined`에 필요한 런타임 핸들러 제공

## 빌드
```bash
make -C libkubsan build CONFIG=debug PLATFORM=pc-x64
```

## 의존성
- `libkc`
