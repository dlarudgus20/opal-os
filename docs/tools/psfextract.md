# psfextract
`psfextract.py`는 폰트를 커널에 포함할 수 있는 PSF2 파일로 변환합니다.

지원 입력:
- C 배열 소스 (`.c`)
- PSF/PSF.GZ (`.psf`, `.psf.gz`)
- TTF/OTF/TTC (`.ttf`, `.otf`, `.ttc`)
- 시스템 폰트 자동 선택:
  - `--psf-system` (`/usr/share/consolefonts`)
  - `--ttf-system` (`fc-list` 기반)

기본 사용:
```bash
python3 tools/psfextract.py <input> <output>
```

주요 옵션:
- `-c`, `--c`: C 배열 입력 강제
- `-p`, `--psf`: PSF 입력 강제
- `--psf-system`: 시스템 PSF 자동 선택 (`output`만 전달)
- `-t`, `--ttf`: TTF/OTF 입력 강제
- `--ttf-system`: 시스템 TTF 자동 선택 (`output`만 전달)
- `--width`, `--height`: glyph 크기
- `--count`: glyph 개수
- `--size`: TTF 렌더링 폰트 크기

예시:
```bash
# C 배열 -> PSF2
python3 tools/psfextract.py -c kernel/src/drivers/ascii_font.c kernel/res/gen/font.psf --width 8 --height 16

# PSF -> PSF2 (앞 128 glyph)
python3 tools/psfextract.py -p /usr/share/consolefonts/Lat15-VGA16.psf.gz kernel/res/gen/font.psf --count 128

# 시스템 PSF 자동 선택
python3 tools/psfextract.py --psf-system kernel/res/gen/font.psf --width 8 --height 16 --count 128
```

노트:
- `--psf-system`은 `/usr/share/consolefonts`가 없는 환경에서 실패할 수 있습니다.
- 커널 기본 빌드에서는 `kernel/res.mk`가 `--width 8 --height 16`을 지정해 `font.psf`를 생성합니다.
