"""Exercise valid and failed CJK font startup in fresh processes."""
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="windoom-language-font-") as tmp:
    temp = Path(tmp)
    exe = temp / "font-test"
    subprocess.run([
        "cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-I" + str(root / "linuxdoom-1.10"),
        str(root / "linuxdoom-1.10/language.c"),
        str(root / "tests/language_font_test.c"), "-o", str(exe)], check=True)
    for case in ("valid", "missing", "corrupt", "duplicate", "offset"):
        game = temp / case
        (game / "languages").mkdir(parents=True)
        shutil.copyfile(root / "languages/zh-cn.ini", game / "languages/zh-cn.ini")
        shutil.copyfile(root / "languages/zh-cn.ini", game / "languages/other.ini")
        if case != "missing":
            font = game / "languages/zh-cn.cjk"
            shutil.copyfile(root / "languages/zh-cn.cjk", font)
            if case == "corrupt":
                data = bytearray(font.read_bytes())
                data[-1] ^= 1
                font.write_bytes(data)
            elif case in ("duplicate", "offset"):
                data = bytearray(font.read_bytes())
                if case == "duplicate":
                    first_cp = data[36:40]
                    data[46:50] = first_cp
                else:
                    struct.pack_into("<I", data, 50, struct.unpack_from("<I", data, 40)[0])
                struct.pack_into("<I", data, 32, zlib.crc32(data[36:]))
                font.write_bytes(data)
        subprocess.run([str(exe), str(game), "valid" if case == "valid" else "invalid"],
                       cwd=temp, check=True)
    game = temp / "other-language"
    (game / "languages").mkdir(parents=True)
    shutil.copyfile(root / "languages/zh-cn.ini", game / "languages/other.ini")
    subprocess.run([str(exe), str(game), "other"], cwd=temp, check=True)
print("CJK font load/fallback tests passed")
