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

## 4) 시스템 PSF 폰트 없음 (`--psf-system` 실패)
증상:
- `python3 tools/psfextract.py --psf-system ...` 실행 시 시스템 폰트를 찾지 못해 실패
- 커널 빌드에서 `res/gen/font.psf` 생성 단계가 실패

원인:
- `/usr/share/consolefonts`가 없거나 비어 있음
- 요청한 `--width/--height/--count` 조건을 만족하는 폰트가 없음

대응:
```bash
# 1) 시스템 폰트 경로 확인
ls /usr/share/consolefonts

# 2) 특정 입력 폰트를 직접 지정
make -C kernel build KERNEL_PSF=/path/to/font.psf.gz

# 3) 또는 시스템 폰트 패키지 설치 후 재시도 (배포판별 패키지명 상이)
```

## 5) `KERNEL_PSF`를 바꿨는데 폰트가 그대로임
증상:
- `KERNEL_PSF=/new/font.psf.gz`로 다시 빌드했는데 기존 폰트가 계속 포함됨

원인:
- 증분 빌드에서 생성 리소스(`res/gen/font.psf`)가 재생성되지 않음

대응:
```bash
make -C kernel clean-gen
make -C kernel build KERNEL_PSF=/path/to/font.psf.gz
```
