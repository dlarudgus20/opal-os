# kernel

`kernel/`은 opal-os 커널 실행 이미지(`kernel.elf`, `kernel.sys`)를 생성합니다.

## 기능
- opal-os 핵심 커널입니다.

## 빌드
```bash
make -C kernel build CONFIG=debug PLATFORM=pc-x64
```

산출물:
- `build/<platform>/<config>/kernel.elf`
- `build/<platform>/<config>/kernel.sys`

## hosted 테스트
```bash
make -C kernel test CONFIG=debug PLATFORM=pc-x64
```

## 커널 유닛 테스트
```bash
# 프로젝트 루트에서
make unit-test CONFIG=debug PLATFORM=pc-x64
```

유닛테스트 경로 산출물:
- `build/unit-test/<platform>/<config>/...`

## 의존성
- 일반 빌드: `libkc`, `libcoll`, `libkubsan`
- hosted 테스트: `libpanicimpl`(shared)

## 커널 리소스
`res.mk`는 `res/gen/`에 필요한 리소스를 생성합니다.

- `res/gen/font.psf`
  - `KERNEL_PSF`가 비어 있으면 `--psf-system`으로 시스템 PSF 자동 선택
  - `KERNEL_PSF`가 지정되면 해당 파일을 `--psf` 입력으로 사용
  - `KERNEL_PSF` 값을 바꾼 뒤 결과를 확실히 갱신하려면 `clean-gen` 후 다시 빌드

  예시:
  ```bash
  # 시스템 PSF 자동 선택
  make -C kernel build CONFIG=debug PLATFORM=pc-x64

  # 특정 PSF 파일 강제
  make -C kernel build CONFIG=debug PLATFORM=pc-x64 KERNEL_PSF=/usr/share/consolefonts/Lat15-VGA16.psf.gz

  # 생성 리소스 정리 후 재생성
  make -C kernel clean-gen
  make -C kernel build CONFIG=debug PLATFORM=pc-x64 KERNEL_PSF=/usr/share/consolefonts/Lat15-VGA16.psf.gz
  ```
