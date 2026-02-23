#!/usr/bin/env python3
from __future__ import annotations

import argparse
import gzip
import pathlib
import re
import struct
import subprocess
import sys
from dataclasses import dataclass


PSF1_MAGIC = b"\x36\x04"
PSF1_MODE512 = 0x01

PSF2_MAGIC = 0x864AB572
PSF2_HEADER_FMT = "<8I"
PSF2_HEADER_SIZE = 32
PSF2_VERSION = 0
DEFAULT_GLYPH_COUNT = 128
DEFAULT_WIDTH = 8
DEFAULT_HEIGHT = 16


@dataclass(frozen=True)
class Font:
    glyphs: int
    charsize: int
    width: int
    height: int
    payload: bytes


def read_maybe_gzip(path: pathlib.Path) -> bytes:
    raw = path.read_bytes()
    if len(raw) >= 2 and raw[:2] == b"\x1f\x8b":
        return gzip.decompress(raw)
    return raw


def parse_psf(data: bytes) -> Font:
    if len(data) < 4:
        raise ValueError("file too small")

    if data[:2] == PSF1_MAGIC:
        mode = data[2]
        charsize = data[3]
        glyphs = 512 if (mode & PSF1_MODE512) else 256
        payload_size = glyphs * charsize
        if len(data) < 4 + payload_size:
            raise ValueError("truncated PSF1 payload")
        payload = data[4 : 4 + payload_size]
        return Font(glyphs, charsize, 8, charsize, payload)

    if len(data) < PSF2_HEADER_SIZE:
        raise ValueError("invalid PSF2 size")
    magic, version, headersize, _flags, glyphs, charsize, height, width = struct.unpack(
        PSF2_HEADER_FMT, data[:PSF2_HEADER_SIZE]
    )
    if magic != PSF2_MAGIC:
        raise ValueError("unsupported format (expected PSF1/PSF2)")
    if version != 0:
        raise ValueError(f"unsupported PSF2 version: {version}")
    if headersize < PSF2_HEADER_SIZE:
        raise ValueError("invalid PSF2 headersize")

    payload_end = headersize + glyphs * charsize
    if len(data) < payload_end:
        raise ValueError("truncated PSF2 payload")
    payload = data[headersize:payload_end]
    return Font(glyphs, charsize, width, height, payload)


def resolve_dimensions(args: argparse.Namespace) -> tuple[int, int]:
    width = DEFAULT_WIDTH if args.width is None else args.width
    height = DEFAULT_HEIGHT if args.height is None else args.height
    return width, height


def write_psf_subset(output_path: pathlib.Path, font: Font, glyph_count: int) -> int:
    payload = font.payload[: glyph_count * font.charsize]
    out = build_psf2(payload, font.width, font.height, glyph_count)
    output_path.write_bytes(out)
    print(
        f"wrote {output_path} "
        f"(glyphs={glyph_count}, width={font.width}, height={font.height}, "
        f"charsize={font.charsize}, bytes={len(out)})"
    )
    return 0


def build_psf2(payload: bytes, width: int, height: int, glyphs: int) -> bytes:
    bytes_per_row = (width + 7) // 8
    charsize = bytes_per_row * height
    if len(payload) != glyphs * charsize:
        raise ValueError("payload size mismatch for PSF2")

    header = struct.pack(
        PSF2_HEADER_FMT,
        PSF2_MAGIC,
        PSF2_VERSION,
        PSF2_HEADER_SIZE,
        0,  # no unicode table
        glyphs,
        charsize,
        height,
        width,
    )
    return header + payload


def parse_c_array_bytes(text: str) -> bytes:
    m = re.search(r"\{(.*)\}", text, re.DOTALL)
    if not m:
        raise ValueError("could not find C array initializer braces")

    body = m.group(1)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.DOTALL)
    body = re.sub(r"//.*?$", "", body, flags=re.MULTILINE)

    values = []
    for tok in re.findall(r"0x[0-9a-fA-F]+|\d+", body):
        v = int(tok, 0)
        if not 0 <= v <= 0xFF:
            raise ValueError(f"byte value out of range: {v}")
        values.append(v)
    if not values:
        raise ValueError("no byte values found in C array")
    return bytes(values)


def infer_layout(total_bytes: int, width: int, height: int, glyphs: int | None) -> tuple[int, int]:
    bytes_per_row = (width + 7) // 8
    charsize = bytes_per_row * height
    if charsize <= 0:
        raise ValueError("invalid width/height; charsize must be > 0")

    if glyphs is None:
        if total_bytes % charsize != 0:
            raise ValueError(
                f"font byte length {total_bytes} is not divisible by charsize {charsize}; "
                "pass --count/--height/--width explicitly"
            )
        glyphs = total_bytes // charsize
    else:
        expected = glyphs * charsize
        if expected != total_bytes:
            raise ValueError(
                f"byte length mismatch: expected {expected} bytes "
                f"(glyphs={glyphs}, charsize={charsize}), got {total_bytes}"
            )
    return glyphs, charsize


def convert_c_to_psf2(args: argparse.Namespace, input_path: pathlib.Path, output_path: pathlib.Path) -> int:
    width, height = resolve_dimensions(args)
    if width <= 0 or height <= 0:
        print("error: width/height must be positive", file=sys.stderr)
        return 2
    if args.count is not None and args.count <= 0:
        print("error: --count must be positive", file=sys.stderr)
        return 2

    text = input_path.read_text(encoding="utf-8")
    payload = parse_c_array_bytes(text)
    bytes_per_row = (width + 7) // 8
    charsize = bytes_per_row * height

    if args.count is None:
        glyphs, charsize = infer_layout(len(payload), width, height, None)
        payload_out = payload
    else:
        required = args.count * charsize
        if required > len(payload):
            print(
                f"error: requested glyph count {args.count} requires {required} bytes, "
                f"but input has {len(payload)} bytes",
                file=sys.stderr,
            )
            return 2
        glyphs = args.count
        payload_out = payload[:required]

    psf = build_psf2(payload_out, width, height, glyphs)
    output_path.write_bytes(psf)
    print(
        f"wrote {output_path} "
        f"(glyphs={glyphs}, width={width}, height={height}, "
        f"charsize={charsize}, bytes={len(psf)})"
    )
    return 0


def convert_psf_to_psf2(args: argparse.Namespace, input_path: pathlib.Path, output_path: pathlib.Path) -> int:
    effective_count = DEFAULT_GLYPH_COUNT if args.count is None else args.count
    if effective_count <= 0:
        print("error: --count must be positive", file=sys.stderr)
        return 2
    font = parse_psf(read_maybe_gzip(input_path))
    if args.width is not None and font.width != args.width:
        print(
            f"error: source width is {font.width}, requested --width {args.width}",
            file=sys.stderr,
        )
        return 2
    if args.height is not None and font.height != args.height:
        print(
            f"error: source height is {font.height}, requested --height {args.height}",
            file=sys.stderr,
        )
        return 2
    if effective_count > font.glyphs:
        print(
            f"error: requested glyph count {effective_count} exceeds source glyphs {font.glyphs}",
            file=sys.stderr,
        )
        return 2
    return write_psf_subset(output_path, font, effective_count)


def list_system_psf_fonts(consolefonts_dir: pathlib.Path) -> list[pathlib.Path]:
    if not consolefonts_dir.exists():
        return []

    candidates: list[pathlib.Path] = []
    for p in sorted(consolefonts_dir.iterdir()):
        if not p.is_file():
            continue
        name = p.name.lower()
        if name.endswith(".psf") or name.endswith(".psf.gz"):
            candidates.append(p)
    return candidates


def score_system_psf_candidate(path: pathlib.Path, font: Font) -> tuple[int, int, int, str]:
    name = path.name.lower()

    if font.width == 8 and font.height == 16:
        res_score = 3
    elif font.width == 8 and font.height == 14:
        res_score = 2
    elif font.width == 8 and font.height == 8:
        res_score = 1
    else:
        res_score = 0

    if "terminus" in name:
        name_score = 3
    elif "vga" in name:
        name_score = 2
    elif "fixed" in name:
        name_score = 1
    else:
        name_score = 0

    # Lowest-priority hint: charset naming.
    if "lat" in name:
        charset_score = 2
    elif "uni" in name:
        charset_score = 1
    else:
        charset_score = 0

    return (res_score, name_score, charset_score, path.name)


def choose_system_psf_font(
    consolefonts_dir: pathlib.Path,
    glyph_count: int,
    required_width: int | None,
    required_height: int | None,
) -> tuple[pathlib.Path, Font]:
    candidates = list_system_psf_fonts(consolefonts_dir)
    if not candidates:
        raise ValueError(f"no PSF fonts found in {consolefonts_dir}")

    parse_failed = 0
    too_small = 0
    wrong_dimension = 0
    valid: list[tuple[tuple[int, int, int, str], pathlib.Path, Font]] = []

    for path in candidates:
        try:
            font = parse_psf(read_maybe_gzip(path))
        except Exception:
            parse_failed += 1
            continue
        if required_width is not None and font.width != required_width:
            wrong_dimension += 1
            continue
        if required_height is not None and font.height != required_height:
            wrong_dimension += 1
            continue
        if font.glyphs < glyph_count:
            too_small += 1
            continue
        valid.append((score_system_psf_candidate(path, font), path, font))

    if not valid:
        raise ValueError(
            f"no usable PSF font in {consolefonts_dir} "
            f"(required glyphs={glyph_count}, required_size={required_width}x{required_height}, "
            f"parse_failed={parse_failed}, too_small={too_small}, wrong_dimension={wrong_dimension})"
        )

    # Highest score wins; ties resolved by filename ascending for determinism.
    valid.sort(key=lambda item: (-item[0][0], -item[0][1], -item[0][2], item[0][3]))
    _, path, font = valid[0]
    return path, font


def convert_psf_system_to_psf2(args: argparse.Namespace, output_path: pathlib.Path) -> int:
    effective_count = DEFAULT_GLYPH_COUNT if args.count is None else args.count
    if effective_count <= 0:
        print("error: --count must be positive", file=sys.stderr)
        return 2

    consolefonts_dir = pathlib.Path("/usr/share/consolefonts")
    source_path, source_font = choose_system_psf_font(
        consolefonts_dir,
        effective_count,
        args.width,
        args.height,
    )
    print(f"source: {source_path}")
    return write_psf_subset(output_path, source_font, effective_count)


def list_system_fonts() -> list[tuple[str, str]]:
    out = subprocess.check_output(["fc-list", ":", "file", "family", "style"], text=True, stderr=subprocess.STDOUT)
    fonts: list[tuple[str, str]] = []
    for line in out.splitlines():
        parts = line.split(":", 2)
        if len(parts) < 2:
            continue
        path = parts[0].strip()
        meta = ":".join(parts[1:]).strip()
        if path:
            fonts.append((path, meta))
    return fonts


def pick_font(fonts: list[tuple[str, str]]) -> tuple[str, str]:
    keywords = ("mono", "code", "console", "fixed", "terminal")
    for path, meta in fonts:
        s = f"{path} {meta}".lower()
        if any(k in s for k in keywords):
            return path, meta
    return fonts[0]


def compute_layout_origin(font, width: int, height: int, glyph_count: int) -> tuple[int, int]:
    min_left = 0
    min_top = 0
    max_right = 0
    max_bottom = 0
    seen = False

    for code in range(glyph_count):
        if code < 0x20 or code == 0x7F:
            continue
        ch = chr(code)
        try:
            l, t, r, b = font.getbbox(ch, anchor="ls")
        except Exception:
            continue
        if l == r or t == b:
            continue
        if not seen:
            min_left, min_top, max_right, max_bottom = l, t, r, b
            seen = True
        else:
            min_left = min(min_left, l)
            min_top = min(min_top, t)
            max_right = max(max_right, r)
            max_bottom = max(max_bottom, b)

    if not seen:
        return 0, height - 1

    box_w = max_right - min_left
    box_h = max_bottom - min_top
    pad_x = max((width - box_w) // 2, 0)
    pad_y = max((height - box_h) // 2, 0)
    return -min_left + pad_x, -min_top + pad_y


def glyph_to_rows(font, code: int, width: int, height: int, origin_x: int, baseline_y: int) -> bytes:
    from PIL import Image, ImageDraw

    bytes_per_row = (width + 7) // 8
    img = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(img)

    if code < 0x20 or code == 0x7F:
        return b"\x00" * (bytes_per_row * height)

    ch = chr(code)
    try:
        draw.text((origin_x, baseline_y), ch, fill=255, font=font, anchor="ls")
    except TypeError:
        draw.text((origin_x, baseline_y - font.size), ch, fill=255, font=font)

    out = bytearray()
    px = img.load()
    for yy in range(height):
        row = bytearray(bytes_per_row)
        for xx in range(width):
            if px[xx, yy] >= 128:
                row[xx // 8] |= 1 << (7 - (xx % 8))
        out.extend(row)
    return bytes(out)


def convert_ttf_to_psf2(
    args: argparse.Namespace,
    output_path: pathlib.Path,
    input_ttf: pathlib.Path | None,
    use_system_ttf: bool,
) -> int:
    width, height = resolve_dimensions(args)
    if width <= 0 or height <= 0 or args.size <= 0:
        print("error: --size/--width/--height must be positive", file=sys.stderr)
        return 2
    try:
        from PIL import ImageFont
    except Exception as exc:
        print(f"error: Pillow is required ({exc})", file=sys.stderr)
        return 2

    glyph_count = DEFAULT_GLYPH_COUNT if args.count is None else args.count
    if glyph_count <= 0:
        print("error: --count must be positive", file=sys.stderr)
        return 2

    if input_ttf is not None:
        font_path = str(input_ttf)
        meta = "input-ttf"
    elif use_system_ttf:
        fonts = list_system_fonts()
        if not fonts:
            print("error: no system fonts found via fc-list", file=sys.stderr)
            return 2
        font_path, meta = pick_font(fonts)
    else:
        print("error: missing ttf input", file=sys.stderr)
        return 2

    try:
        font = ImageFont.truetype(font_path, args.size)
    except Exception as exc:
        print(f"error: failed to load font '{font_path}': {exc}", file=sys.stderr)
        return 2

    origin_x, baseline_y = compute_layout_origin(font, width, height, glyph_count)
    rows = [
        glyph_to_rows(font, code, width, height, origin_x, baseline_y)
        for code in range(glyph_count)
    ]
    payload = b"".join(rows)
    out = build_psf2(payload, width, height, glyph_count)
    output_path.write_bytes(out)

    charsize = ((width + 7) // 8) * height
    print(f"font: {font_path}")
    print(f"meta: {meta}")
    print(
        f"wrote {output_path} "
        f"(glyphs={glyph_count}, width={width}, height={height}, "
        f"charsize={charsize}, bytes={len(out)})"
    )
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="PSF extractor (auto mode by extension: .c / .psf(.gz) / .ttf, or force with options)"
    )
    mode = p.add_mutually_exclusive_group()
    mode.add_argument("-c", "--c", action="store_true", dest="mode_c", help="force C-array(.c) -> PSF2 mode")
    mode.add_argument("-p", "--psf", action="store_true", dest="mode_psf", help="force PSF -> PSF2 subset mode")
    mode.add_argument("--psf-system", action="store_true", help="use auto-picked system PSF from /usr/share/consolefonts")
    mode.add_argument("-t", "--ttf", action="store_true", dest="mode_ttf", help="force TTF/OTF -> PSF2 mode")
    mode.add_argument("--ttf-system", action="store_true", help="use auto-picked system TTF (no input path)")

    p.add_argument("paths", nargs="+", type=pathlib.Path, help="[input] output, or output with --ttf-system/--psf-system")
    p.add_argument("--width", type=int, default=None, help=f"glyph width in pixels (default: {DEFAULT_WIDTH})")
    p.add_argument("--height", type=int, default=None, help=f"glyph height in pixels (default: {DEFAULT_HEIGHT})")
    p.add_argument(
        "--count",
        type=int,
        default=None,
        help=f"glyph count (default: all glyphs in C, {DEFAULT_GLYPH_COUNT} in PSF/TTF/system-PSF)",
    )
    p.add_argument("--size", type=int, default=16, help="font point size (TTF mode)")
    return p


def detect_mode(args: argparse.Namespace, input_path: pathlib.Path | None) -> str:
    forced_count = int(args.mode_c) + int(args.mode_psf) + int(args.mode_ttf) + int(args.ttf_system) + int(args.psf_system)
    if forced_count > 1:
        raise ValueError("only one of --c/--psf/--psf-system/--ttf/--ttf-system can be specified")
    if args.mode_c:
        return "c"
    if args.mode_psf:
        return "psf"
    if args.psf_system:
        return "psf-system"
    if args.mode_ttf:
        return "ttf"
    if args.ttf_system:
        return "ttf-system"

    if input_path is None:
        raise ValueError("cannot auto-detect mode without input path; use --ttf-system or --psf-system")

    name = input_path.name.lower()
    suffixes = [s.lower() for s in input_path.suffixes]
    if name.endswith(".psf.gz"):
        return "psf"
    if suffixes and suffixes[-1] == ".c":
        return "c"
    if suffixes and suffixes[-1] == ".psf":
        return "psf"
    if suffixes and suffixes[-1] in {".ttf", ".otf", ".ttc"}:
        return "ttf"
    raise ValueError("unsupported input extension; use --c/--psf/--psf-system/--ttf/--ttf-system")


def parse_paths(args: argparse.Namespace) -> tuple[pathlib.Path | None, pathlib.Path]:
    if args.ttf_system or args.psf_system:
        if len(args.paths) != 1:
            raise ValueError("with --ttf-system/--psf-system, pass only output path")
        return None, args.paths[0]

    if len(args.paths) != 2:
        raise ValueError("pass input and output path")
    return args.paths[0], args.paths[1]


def validate_io_paths(mode: str, input_path: pathlib.Path | None, output_path: pathlib.Path) -> None:
    if mode not in {"ttf-system", "psf-system"}:
        assert input_path is not None
        if not input_path.exists():
            raise FileNotFoundError(f"input file not found: {input_path}")
        if input_path.is_dir():
            raise IsADirectoryError(f"input path is a directory: {input_path}")

    if output_path.exists() and output_path.is_dir():
        raise IsADirectoryError(f"output path is a directory: {output_path}")

    out_dir = output_path.parent
    if not out_dir.exists():
        raise FileNotFoundError(f"output directory does not exist: {out_dir}")
    if not out_dir.is_dir():
        raise NotADirectoryError(f"output directory is not a directory: {out_dir}")


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    try:
        input_path, output_path = parse_paths(args)
        mode = detect_mode(args, input_path)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    try:
        validate_io_paths(mode, input_path, output_path)

        if mode == "c":
            assert input_path is not None
            return convert_c_to_psf2(args, input_path, output_path)
        if mode == "psf":
            assert input_path is not None
            return convert_psf_to_psf2(args, input_path, output_path)
        if mode == "ttf":
            assert input_path is not None
            return convert_ttf_to_psf2(args, output_path, input_path, False)
        if mode == "psf-system":
            return convert_psf_system_to_psf2(args, output_path)
        if mode == "ttf-system":
            return convert_ttf_to_psf2(args, output_path, None, True)

        print(f"error: unknown mode {mode}", file=sys.stderr)
        return 2
    except (FileNotFoundError, NotADirectoryError, IsADirectoryError, PermissionError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
