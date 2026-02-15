# libkc

`libkc`는 커널/라이브러리 공통으로 사용하는 기본 C 런타임 성격의 함수 집합입니다.

## 역할
- 문자열/메모리 함수 (`mem*`, `str*`)
- 정렬/유틸 (`align_ceil_sz_p2`, `sort`, `container_of`)
- 포맷 엔진 (`fmt_*`)
- `snprintf_s`/`vsnprintf_s`
- assert/panic 매크로 지원 헤더

## 공개 헤더 (`<kc/...>`)
- `kc/string.h`
- `kc/stdlib.h`
- `kc/stdio.h`
- `kc/fmt.h`
- `kc/assert.h`
- `kc/ctype.h`, `kc/attributes.h`

## 빌드
```bash
make -C libkc build CONFIG=debug PLATFORM=pc-x64
```

기본 타깃은 정적 라이브러리:
- `build/pc-x64/<config>/libkc.a`

테스트 모드에서는 shared로도 사용됩니다.

## 테스트
```bash
make -C libkc test CONFIG=debug PLATFORM=pc-x64
```

`TEST_DO_NOT_LINK=1` 설정으로, 테스트 실행 파일은 필요 시 `dlopen` 기반 시나리오를 사용합니다.

## 테스트 링크 주의사항 (`--exclude-libs`)
`libkc`는 `memcpy`, `memset`, `strlen` 같은 libc 유사 심볼을 제공합니다.  
다른 모듈의 hosted 테스트에서 `libkc.a`를 shared 타깃에 링크할 때는 심볼 export 충돌을 막기 위해 아래 플래그를 권장합니다.

```make
LDFLAGS_ON_TEST := -Wl,--exclude-libs=libkc
```

대표 적용 위치:
- `kernel/Makefile`
- `libcoll/makefile`
