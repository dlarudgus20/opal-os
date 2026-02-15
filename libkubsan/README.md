# libkubsan

`libkubsan`은 freestanding 커널 빌드에서 UBSAN 핸들러 심볼을 제공하는 정적 라이브러리입니다.

## 역할
- 커널(`-fsanitize=undefined`)에서 필요한 런타임 핸들러 구현
- 사용자 공간 sanitizer 런타임에 의존하지 않는 최소 구현

## 빌드
```bash
make -C libkubsan build CONFIG=debug PLATFORM=pc-x64
```

산출물:
- `build/pc-x64/<config>/libkubsan.a`

## 의존성
- 정적 참조: `libkc`
