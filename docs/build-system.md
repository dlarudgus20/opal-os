# Build System

이 문서는 루트 Makefile, 공통 설정, 공통 규칙, 서브프로젝트 Makefile이 빌드 산출물을 만드는 과정을 설명합니다. ([루트 Makefile](../Makefile), [공통 설정](../mkfiles/config.mk), [공통 규칙](../mkfiles/rules.mk))

## 1. 루트 타깃

| 명령 | 동작 |
| --- | --- |
| `make`, `make build` | `kernel/`에서 `kernel.elf`와 `kernel.sys`를 빌드합니다. |
| `make iso` | `kernel.sys`와 `grub.cfg`를 담은 GRUB 부팅 ISO를 생성합니다. |
| `make run` | ISO를 갱신한 뒤 QEMU를 실행합니다. |
| `make test` | `TEST=1`로 `run`을 호출해 QEMU에서 커널 테스트를 실행합니다. |
| `make disk-images` | QEMU에 연결할 qcow2 디스크 이미지 세 개를 생성합니다. |
| `make gen` | 각 서브프로젝트의 `RESOURCES`에 선언된 파일을 생성합니다. |
| `make clean-gen` | 각 서브프로젝트의 `res/gen/`을 정리합니다. |
| `make clean` | 현재 플랫폼·구성의 일반 빌드 산출물을 정리합니다. |
| `make clean-test` | `TEST=1`로 `clean`을 호출해 현재 플랫폼·구성의 테스트 산출물을 정리합니다. |
| `make fullclean` | 루트와 빌드 대상 서브프로젝트의 `build/` 전체를 정리합니다. |

생성과 정리를 순회하는 서브프로젝트는 루트 Makefile의 `SUBDIRS`에 정의합니다. 루트의 `clean`과 `fullclean`은 각 서브프로젝트에 호출된 타깃 이름을 전달한 뒤 루트 산출물을 정리합니다. `clean`, `clean-test`, `fullclean`은 서브프로젝트의 공용 `res/gen/`도 정리합니다.

실행 옵션은 일반 실행과 테스트 실행에 공통으로 적용됩니다.

| 변수 | 동작 |
| --- | --- |
| `QEMU_FLAGS` | 기본값은 `-m 128 -serial stdio -no-reboot -boot order=dc`입니다. |
| `QEMU_HDDS` | 루트에 존재하는 디스크 이미지로 QEMU 디스크 인자를 구성합니다. |
| `QEMU_DISPNONE=1` | `-display none`을 추가합니다. |
| `UEFI=1` | `-bios $(UEFI_FIRMWARE)`를 추가해 UEFI 펌웨어로 실행합니다. |
| `UEFI_FIRMWARE` | 기본 경로는 `/usr/share/ovmf/OVMF.fd`입니다. |

`QEMU_FLAGS`를 명령줄에서 직접 지정하면 위 기본값과 옵션별 추가 인자가 함께 대체됩니다. 시리얼 출력을 터미널에서 보려면 `-serial stdio`를 넣고, ISO를 먼저 부팅하려면 `-boot order=dc`를 넣습니다. 화면 없이 실행할 때는 `-display none`도 직접 지정합니다.

### 1.1 ISO 생성 경로

`make iso`는 `kernel.sys`, `iso/boot/grub/grub.cfg`, 루트 Makefile을 ISO의 의존성으로 사용합니다. `kernel.sys`의 갱신 여부는 커널 서브 Make가 판단합니다.

ISO를 다시 생성할 때는 다음 순서로 작업합니다.

1. `$(BUILD_DIR)/iso/`를 비우고 `boot/grub/` 디렉터리를 준비합니다.
2. `grub.cfg`를 `boot/grub/grub.cfg`에, `kernel.sys`를 `boot/kernel.sys`에 복사합니다.
3. `grub-mkrescue`로 `$(BUILD_DIR)/opal-os.iso`를 생성합니다.

복사 대상은 루트 Makefile에 명시되어 있습니다. ISO에 파일을 추가할 때는 해당 파일을 ISO 타깃의 의존성과 복사 규칙에 함께 등록합니다. GRUB은 `multiboot2 /boot/kernel.sys`로 커널을 실행합니다. [GRUB 설정](../iso/boot/grub/grub.cfg)

### 1.2 디스크 이미지 생성과 연결

`make disk-images`는 루트에 qcow2 이미지를 생성합니다. 일반 실행과 테스트 실행은 루트에 존재하는 이미지만 연결합니다.

| 이미지 | 생성 크기 | 연결 인자 |
| --- | --- | --- |
| `hda.img` | 32M | `-hda hda.img` |
| `hdb.img` | 16M | `-hdb hdb.img` |
| `hdd.img` | 8M | `-hdd hdd.img` |

## 2. 공통 설정 (`mkfiles/config.mk`)

| 변수 | 기본값 | 의미 |
| --- | --- | --- |
| `CONFIG` | `debug` | `debug` 또는 `release`를 선택합니다. |
| `PLATFORM` | `pc-x64` | 지원 플랫폼은 `pc-x64`입니다. |
| `ARCH` | `x86_64` | 지원 아키텍처는 `x86_64`입니다. |
| `TEST` | 미설정 | `1`이면 테스트 구성으로 빌드합니다. |
| `TOOLSET_PREFIX` | `x86_64-elf` | GCC 및 Binutils 실행 파일 이름의 접두사입니다. |
| `NO_ANALYZER` | 미설정 | `1`이면 디버그 빌드의 GCC 분석기를 비활성화합니다. |
| `NO_SANITIZE` | 미설정 | `1`이면 디버그 빌드의 UBSAN 계측을 비활성화합니다. |

공통 설정은 `CONFIG`, `PLATFORM`, `ARCH`, `TEST`를 서브 Make에 전달합니다. C는 C23 freestanding 모드로 컴파일하고, 어셈블리는 NASM으로 ELF64 오브젝트를 생성합니다.

### 2.1 C 전처리 매크로

- 공통 빌드는 `OPAL_CONFIG`, `OPAL_PLATFORM`을 문자열로 정의합니다.
- 디버그는 `DEBUG`를 정의합니다. 릴리스는 `NDEBUG`를 정의하고 `-O3 -flto=auto`를 적용합니다.
- 실행 파일을 `TEST=1`로 빌드하면 `OPAL_TEST`를 추가로 정의합니다.

### 2.2 분석기와 sanitizer

- 디버그 C 빌드는 기본으로 `-fanalyzer`, `-fsanitize=undefined`, `-fno-omit-frame-pointer`를 적용합니다.
- `NO_ANALYZER=1`과 `NO_SANITIZE=1`은 각각 `-fanalyzer`와 `-fsanitize=undefined`만 제외합니다.
- freestanding 빌드에서 사용하는 UBSAN 핸들러는 `libkubsan`에서 제공합니다.

### 2.3 빌드 경로

| 산출물 | 일반 빌드 | 테스트 빌드 |
| --- | --- | --- |
| 서브프로젝트 산출물 | `<project>/build/<platform>/<config>/` | `<project>/build/test/<platform>/<config>/` |
| ISO | `build/<platform>/<config>/` | `build/test/<platform>/<config>/` |
| `STATIC_REFS`로 참조하는 정적 라이브러리 | `<lib>/build/<platform>/<config>/` | 같은 일반 빌드 경로를 재사용합니다. |

`BUILD_DIR`은 현재 빌드의 출력 경로를 지정합니다. `BUILD_DIR_REF`는 정적 라이브러리 참조 경로를 지정하며, 테스트 모드에서도 일반 빌드 경로를 사용합니다.

## 3. 공통 규칙 (`mkfiles/rules.mk`)

### 3.1 소스 자동 수집

- 각 프로젝트의 `src/`와 `platform/$(PLATFORM)/src/`에서 C와 NASM 소스를 재귀적으로 수집합니다.
- `TEST=1`이면 `tests/`와 `platform/$(PLATFORM)/tests/`가 추가됩니다.
- C 헤더는 프로젝트의 `include/`, `platform/$(PLATFORM)/include/`, 참조 프로젝트 헤더 경로에서 검색합니다. 프로젝트에 추가 검색 경로가 필요하면 `INCLUDES`로 지정할 수 있습니다.
- 컴파일러와 NASM이 생성한 `.d` 파일을 `-include`로 읽습니다. 오브젝트는 소스와 헤더의 변경 외에 해당 빌드에서 읽은 Makefile의 변경에도 갱신됩니다.

### 3.2 타깃 타입과 링크

- `TARGET_TYPE := executable`은 `LD_SCRIPT`를 사용해 `$(TARGET_NAME).elf`를 링크합니다. 프로젝트는 Makefile이나 그 Makefile이 읽는 플랫폼 설정에서 `LD_SCRIPT`를 지정합니다.
- `TARGET_TYPE := static-lib`는 오브젝트를 모아 `$(TARGET_NAME).a`를 생성합니다.
- 실행 파일의 `-Wl,-u,<symbol>` 인자는 공통 설정의 `LIBKC_BUILTIN_SYMBOLS` 목록에서 생성됩니다. LTO 중 컴파일러가 호출할 수 있는 `memcpy`, `memset` 등 `libkc` 심볼을 링크 결과에 유지합니다. 이 목록은 Make 변수로 재정의할 수 있습니다.

### 3.3 테스트 모드 동작

- `TEST=1`은 테스트 소스를 빌드에 추가하고 실행 파일에 테스트 구성을 적용합니다. 모든 구성에서 컴파일할 공용 코드는 `src/`에, 테스트 구성에서만 컴파일할 코드는 `tests/` 또는 `platform/$(PLATFORM)/tests/`에 둡니다.
- 루트의 `make test`는 테스트용 ISO를 갱신하고 QEMU를 실행합니다. ISO만 만들 때는 `make iso TEST=1`을 사용합니다. 테스트 등록과 실행기의 동작은 별도 문서를 참고합니다. [커널 테스트](kernel-test.md)

### 3.4 참조 프로젝트

- `STATIC_REFS`는 참조 프로젝트의 헤더 경로와 `.a` 링크 의존성을 추가합니다. 아카이브를 확인할 때 서브 Make는 `TEST=0`으로 실행되며, 최신 아카이브를 재사용합니다.
- `INCLUDE_REFS`는 참조 프로젝트의 `include/`와 플랫폼별 헤더 경로를 추가합니다. 이 참조는 컴파일러의 헤더 검색 경로에만 반영됩니다.

### 3.5 빌드 시스템 제한사항

- 같은 구성의 출력 디렉터리를 갱신하는 독립 `make` 프로세스는 순차 실행합니다. 테스트와 일반 빌드의 출력 경로는 분리되며, 두 구성에서 참조하는 정적 라이브러리는 일반 빌드 경로를 공유합니다.
- `make -j clean build`처럼 정리와 빌드를 병렬로 요청하면 생성 중인 산출물이 삭제될 수 있습니다.

## 4. 서브프로젝트 설정 패턴

서브프로젝트는 `TARGET_NAME`, `TARGET_TYPE`과 필요한 참조를 선언한 뒤 공통 설정과 규칙을 포함합니다. 플랫폼별 설정이 있다면 공통 설정으로 `PLATFORM`을 결정한 뒤 읽습니다. 다음은 플랫폼 설정에서 링크 스크립트를 지정하는 실행 파일 프로젝트의 예시입니다.

```makefile
TARGET_NAME         := <프로젝트 이름>
TARGET_TYPE         := executable
STATIC_REFS         := <링크할 정적 라이브러리>
INCLUDE_REFS        := <헤더를 참조할 프로젝트>

include ../mkfiles/config.mk
-include platform/$(PLATFORM)/config.mk
include ../mkfiles/rules.mk
```

정적 라이브러리를 만드는 프로젝트에는 `TARGET_TYPE := static-lib`를 사용합니다. 프로젝트별 산출물을 추가하려면 공통 규칙을 포함한 뒤 `$(TARGET)`에 의존하는 규칙을 정의하고 `build`의 의존성으로 추가합니다.

## 5. 디버그 산출물

| 파일 | 내용 |
| --- | --- |
| `<target>.map` | 링크된 오브젝트, 아카이브, 섹션의 배치입니다. |
| `<target>.nm` | 실행 파일의 심볼 목록과 주소순으로 정리한 심볼 목록입니다. |
| `<target>.sym` | bochsdbg가 인식할 수 있는 심볼 목록입니다. |
| `<target>.disasm` | 링크된 실행 파일의 디스어셈블리입니다. |
| 오브젝트별 `.dump`, NASM의 `.lst` | 오브젝트 디스어셈블리와 어셈블러 리스팅입니다. |
| 정적 라이브러리별 `.a.nm` | 아카이브에 포함된 심볼 목록입니다. |

## 6. 자주 쓰는 명령

```bash
make build CONFIG=debug PLATFORM=pc-x64 >/dev/null
make iso CONFIG=release >/dev/null
make iso TEST=1 >/dev/null
make test QEMU_DISPNONE=1
make clean-test >/dev/null
make gen >/dev/null
make clean-gen >/dev/null
```

기본 테스트 실행에서는 결과 출력 후 QEMU가 자동으로 종료됩니다. 시리얼 로그의 `==== test end ====` 요약과 `make test`의 종료 상태를 확인합니다. 현재 구성에서 어떤 소스를 어떻게 빌드하는지 확인하려면 다음 명령을 사용합니다.

```bash
make -B -n build TEST=1
```

## 7. 리소스 빌드 패턴

생성 리소스가 필요한 프로젝트는 `res.mk`에 `RESOURCES` 목록과 생성 규칙을 정의하고, 프로젝트 Makefile에서 공통 규칙보다 먼저 읽습니다. `make gen`은 목록에 선언된 파일을 생성하고 `make clean-gen`은 `res/gen/`을 정리합니다.

빌드에서 리소스를 소비하는 오브젝트에 생성 파일을 선행 의존성으로 추가합니다. 입력 파일과 변환기 자체도 생성 규칙의 의존성에 포함하면 두 파일의 변경이 증분 빌드에 반영됩니다. C의 `#embed`와 NASM의 `%include` 또는 `incbin`은 `res/`를 추가 검색 경로로 사용합니다.

아래 예시는 입력 파일을 변환해 생성한 바이너리를 오브젝트에 포함하는 프로젝트의 `res.mk`입니다.

<!-- markdownlint-disable MD010 -->
```makefile
RESOURCES           := res/gen/example.bin

$(BUILD_DIR)/src/res.c.o: $(RESOURCES)

res/gen/example.bin: $(INPUT_FILE) res.mk ../tools/some-generator.py
	@mkdir -p $(dir $@)
	$(PYTHON) ../tools/some-generator.py "$<" "$@"
```
<!-- markdownlint-enable MD010 -->

프로젝트별 리소스와 도구 사용법은 각 문서를 참고합니다. [커널 리소스](../kernel/README.md#리소스), [빌드 보조 도구](../tools/README.md)
