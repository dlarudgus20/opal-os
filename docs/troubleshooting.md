# Troubleshooting

## 1) LeakSanitizer 종료 (`ptrace` 제약)
증상:
- 테스트 종료 시 LeakSanitizer가 fatal 에러를 내고 실패

원인:
- 실행 환경이 `ptrace` 제약(디버거/추적 환경) 하에 있어 LSan 동작이 제한됨

대응:
```bash
ASAN_OPTIONS=detect_leaks=0 make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

## 2) 테스트 링크 충돌 (`libkc` 심볼 노출)
증상:
- hosted 테스트 shared 링크 시 `memcpy/memset/strlen` 류 충돌

원인:
- 정적 `libkc.a`를 shared 타깃에 링크할 때 심볼이 외부로 export될 수 있음

대응:
- 테스트 링크 플래그에 `--exclude-libs=libkc` 적용
- `LDFLAGS_ON_TEST := -Wl,--exclude-libs=libkc`

확인:
```bash
objdump -T <target>.so | rg " memcpy$| memmove$| memset$| strlen$"
```

## 3) 실행 시 GUI 에러
증상:
- `make run` 혹은 `make unit-test`에서 `gtk initialization failed`

원인:
- QEMU GUI 백엔드 실행 환경 문제

대응:
- `QEMU_FLAGS="-display none" make run`처럼 GUI 비활성화
