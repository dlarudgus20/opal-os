# kernel

`make -C kernel build`는 `build/pc-x64/debug/`에 `kernel.elf`와 `kernel.sys`를 생성합니다. 릴리스 빌드에는 `CONFIG=release`를 사용합니다.

`platform/pc-x64/src/boot/boot.asm`이 64비트 모드로 전환해 `src/kmain.c`의 `kmain()`을 호출합니다. 자료구조의 구현과 헤더는 `src/utils/`와 `include/opal/utils/`에 있습니다.

테스트 빌드에는 `TEST=1`을 사용합니다. 루트에서 `make test`를 실행하면 QEMU의 COM1에서 결과를 확인할 수 있습니다. [커널 테스트](../docs/kernel-test.md)

## 리소스

`make -C kernel gen`은 `res/gen/font.psf`를 생성합니다. `KERNEL_PSF`로 변환할 PSF 파일을 지정할 수 있습니다. 생성 파일은 `make -C kernel clean-gen`으로 정리합니다. [리소스 빌드 규칙](../docs/build-system.md#7-리소스-빌드-패턴)
