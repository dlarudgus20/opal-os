# Troubleshooting

프로젝트에서 자주 맞닥뜨리는 빌드/테스트 이슈와 점검 순서를 정리합니다.

## 1) LeakSanitizer 종료 (`ptrace` 제약)
증상:
- 테스트 종료 시 LeakSanitizer가 fatal 에러를 내고 실패

원인:
- 실행 환경이 `ptrace` 제약(디버거/추적 환경) 하에 있어 LSan 동작이 제한됨

대응:
```bash
ASAN_OPTIONS=detect_leaks=0 make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

참고:
- 메모리 누수 자체를 완전히 포기하는 의미가 아니라, 환경 제약 구간에서 LSan만 비활성화하는 우회입니다.

## 2) 테스트 링크 충돌 (`libkc` 심볼 노출)
증상:
- 테스트용 shared 객체에서 `memcpy/memmove/memset/strlen...` 류 심볼 충돌
- sanitizer/interceptor와 충돌하거나 의도치 않은 심볼 결합 발생

원인:
- 정적 `libkc.a`를 shared 타깃에 링크할 때 심볼이 외부로 export될 수 있음

대응:
- 테스트 링크 플래그에 `--exclude-libs=libkc` 적용
- 예시: `LDFLAGS_ON_TEST := -Wl,--exclude-libs=libkc`

확인:
```bash
objdump -T <target>.so | rg " memcpy$| memmove$| memset$| strlen$"
```

## 3) 의존 라이브러리 참조 경로가 꼬일 때
증상:
- 서브프로젝트 링크 시 `../<ref>/...` 산출물을 찾지 못함

점검 포인트:
- `mkfiles/conf.mk`의 `BUILD_DIR` / `BUILD_DIR_REF`
- `mkfiles/rules.mk`의 `REFS_STATIC_FILES` / `REFS_SHARED_FILES`
- `IS_TEST`, `UNIT_TEST`, `CONFIG`, `PLATFORM` 조합

권장:
- 문제 재현 시 먼저 `make clean` 또는 `make fullclean` 후 재빌드
