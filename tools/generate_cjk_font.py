#!/usr/bin/env python3
"""Build a 12x12 monochrome glyph subset from the bundled SC Noto face.

Requires Python 3 and the system FreeType shared library for offline generation.
The game runtime does not link to FreeType or read the source font.
"""
import ctypes as C
import ctypes.util
import argparse
import hashlib
import re
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / "languages/zh-cn.ini"
OUTPUT = ROOT / "languages/zh-cn.cjk"
FONT_SHA256 = "b76b0433203017ca80401b2ee0dd69350349871c4b19d504c34dbdd80541690a"
FACE_INDEX = 2  # TTC face list: JP=0, KR=1, Simplified Chinese=2.
PIXEL_SIZE = 10
TILE = 12
MAGIC = b"WDCJK1\0\0"
HEADER = struct.Struct("<8sHHHHHHIIII")
ENTRY = struct.Struct("<IIH")

class Bitmap(C.Structure):
    _fields_ = [("rows", C.c_uint), ("width", C.c_uint), ("pitch", C.c_int),
                ("buffer", C.POINTER(C.c_ubyte)), ("num_grays", C.c_ushort),
                ("pixel_mode", C.c_ubyte), ("palette_mode", C.c_ubyte),
                ("palette", C.c_void_p)]

class FaceRec(C.Structure):
    pass

class SlotRec(C.Structure):
    pass

Face = C.POINTER(FaceRec)
Slot = C.POINTER(SlotRec)
FaceRec._fields_ = [
    ("num_faces", C.c_long), ("face_index", C.c_long),
    ("face_flags", C.c_long), ("style_flags", C.c_long),
    ("num_glyphs", C.c_long), ("family_name", C.c_char_p),
    ("style_name", C.c_char_p), ("num_fixed_sizes", C.c_int),
    ("available_sizes", C.c_void_p), ("num_charmaps", C.c_int),
    ("charmaps", C.c_void_p), ("generic_data", C.c_void_p),
    ("generic_finalizer", C.c_void_p), ("bbox", C.c_long * 4),
    ("units_per_EM", C.c_ushort), ("ascender", C.c_short),
    ("descender", C.c_short), ("height", C.c_short),
    ("max_advance_width", C.c_short), ("max_advance_height", C.c_short),
    ("underline_position", C.c_short), ("underline_thickness", C.c_short),
    ("glyph", Slot), ("size", C.c_void_p), ("charmap", C.c_void_p)]
class Vector(C.Structure):
    _fields_ = [("x", C.c_long), ("y", C.c_long)]
class Metrics(C.Structure):
    _fields_ = [(name, C.c_long) for name in
                ("width", "height", "horiBearingX", "horiBearingY",
                 "horiAdvance", "vertBearingX", "vertBearingY", "vertAdvance")]
SlotRec._fields_ = [
    ("library", C.c_void_p), ("face", Face), ("next", Slot),
    ("glyph_index", C.c_uint), ("generic_data", C.c_void_p),
    ("generic_finalizer", C.c_void_p), ("metrics", Metrics),
    ("linearHoriAdvance", C.c_long), ("linearVertAdvance", C.c_long),
    ("advance", Vector), ("format", C.c_ulong), ("bitmap", Bitmap),
    ("bitmap_left", C.c_int), ("bitmap_top", C.c_int)]

def required_codepoints():
    text = PACK.read_text(encoding="utf-8")
    strings = text.split("[strings]", 1)[1]
    values = []
    for line in strings.splitlines():
        line = line.strip()
        if not line or line.startswith((";", "#")):
            continue
        _, value = line.split("=", 1)
        values.append(value.replace(r"\n", "\n").replace(r"\t", "\t").replace(r"\\", "\\"))
    return sorted({ord(ch) for value in values for ch in value if ord(ch) > 0x7f})

def load_free_type():
    path = ctypes.util.find_library("freetype")
    if not path:
        raise SystemExit("FreeType shared library is required for offline font generation")
    ft = C.CDLL(path)
    ft.FT_Init_FreeType.argtypes = [C.POINTER(C.c_void_p)]
    ft.FT_Init_FreeType.restype = C.c_int
    ft.FT_New_Face.argtypes = [C.c_void_p, C.c_char_p, C.c_long, C.POINTER(Face)]
    ft.FT_New_Face.restype = C.c_int
    ft.FT_Set_Pixel_Sizes.argtypes = [Face, C.c_uint, C.c_uint]
    ft.FT_Set_Pixel_Sizes.restype = C.c_int
    ft.FT_Load_Char.argtypes = [Face, C.c_ulong, C.c_int]
    ft.FT_Load_Char.restype = C.c_int
    return ft

def bitmap_bytes(ft, face, codepoint):
    if ft.FT_Load_Char(face, codepoint, 4) != 0:
        raise ValueError(f"FreeType could not rasterize U+{codepoint:04X}")
    slot = face.contents.glyph.contents
    bmp = slot.bitmap
    if not slot.glyph_index or not bmp.width or not bmp.rows or bmp.pixel_mode not in (1, 2):
        raise ValueError(f"missing/unsupported glyph U+{codepoint:04X}")
    minimum_pitch = (bmp.width + 7) // 8 if bmp.pixel_mode == 1 else bmp.width
    if bmp.width > TILE or bmp.rows > TILE or abs(bmp.pitch) < minimum_pitch:
        raise ValueError(f"glyph does not fit {TILE}x{TILE}: U+{codepoint:04X}")
    canvas = bytearray(TILE * TILE)
    left = slot.bitmap_left
    top = 10 - slot.bitmap_top  # common 12px line baseline
    ink_count = 0
    for sy in range(bmp.rows):
        row = sy if bmp.pitch >= 0 else bmp.rows - 1 - sy
        for sx in range(bmp.width):
            if bmp.pixel_mode == 1:
                byte = bmp.buffer[row * abs(bmp.pitch) + sx // 8]
                ink = bool(byte & (0x80 >> (sx & 7)))
            else:
                ink = bmp.buffer[row * abs(bmp.pitch) + sx] >= 96
            dx, dy = left + sx, top + sy
            if ink:
                if not (0 <= dx < TILE and 0 <= dy < TILE):
                    raise ValueError(f"glyph ink clips 12x12 tile: U+{codepoint:04X}")
                canvas[dy * TILE + dx] = 1
                ink_count += 1
    if not ink_count:
        raise ValueError(f"empty glyph U+{codepoint:04X}")
    packed = bytearray((TILE * TILE + 7) // 8)
    for i, ink in enumerate(canvas):
        if ink:
            packed[i // 8] |= 0x80 >> (i & 7)
    return bytes(packed)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--font", type=Path,
                        default=Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"),
                        help="Noto Sans CJK collection from the licensed Debian font package")
    parser.add_argument("--output", type=Path, default=OUTPUT)
    args = parser.parse_args()
    font_path = args.font.resolve()
    if hashlib.sha256(font_path.read_bytes()).hexdigest() != FONT_SHA256:
        raise SystemExit("Noto source hash does not match the documented Debian font asset")
    codepoints = required_codepoints()
    ft = load_free_type()
    library = C.c_void_p()
    if ft.FT_Init_FreeType(C.byref(library)):
        raise SystemExit("FT_Init_FreeType failed")
    face = Face()
    if ft.FT_New_Face(library, str(font_path).encode(), FACE_INDEX, C.byref(face)):
        raise SystemExit("could not open Noto Sans CJK SC face index 2")
    if face.contents.family_name != b"Noto Sans CJK SC":
        raise SystemExit(f"unexpected face name: {face.contents.family_name!r}")
    major, minor, patch = C.c_int(), C.c_int(), C.c_int()
    ft.FT_Library_Version.argtypes = [C.c_void_p, C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_int)]
    ft.FT_Library_Version.restype = None
    ft.FT_Library_Version(library, C.byref(major), C.byref(minor), C.byref(patch))
    if (major.value, minor.value, patch.value) != (2, 13, 3):
        raise SystemExit("reproducible generation requires FreeType 2.13.3")
    if ft.FT_Set_Pixel_Sizes(face, 0, PIXEL_SIZE):
        raise SystemExit("FT_Set_Pixel_Sizes failed")
    glyphs = [bitmap_bytes(ft, face, cp) for cp in codepoints]
    index_offset = HEADER.size
    data_offset = index_offset + len(codepoints) * ENTRY.size
    index = bytearray()
    data = b"".join(glyphs)
    for i, _cp in enumerate(codepoints):
        index += ENTRY.pack(_cp, data_offset + i * len(glyphs[0]), len(glyphs[0]))
    payload = bytes(index) + data
    file_size = HEADER.size + len(payload)
    header = HEADER.pack(MAGIC, 1, len(codepoints), TILE, TILE, TILE, 0,
                         index_offset, data_offset, file_size, zlib.crc32(payload))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(header + payload)
    print(f"wrote {args.output}: {len(codepoints)} glyphs, {file_size} bytes; FreeType 2.13.3")

if __name__ == "__main__":
    main()
