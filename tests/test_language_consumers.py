"""Compile the actual engine consumer functions and exercise observable feedback.

No engine implementation is copied. Linker GC retains option/pickup/HUD/prompt
paths; this harness replaces only external platform sound/removal/draw output.
Windows consumer code is enabled on portable engine translation units, while
language filesystem operations use the native host implementation.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
cc = os.environ.get("CC", "cc")
with tempfile.TemporaryDirectory(prefix="windoom-language-consumers-") as tmp:
    temp = Path(tmp)
    objects = []
    for source, windows in (
        ("linuxdoom-1.10/language.c", False),
        ("linuxdoom-1.10/m_menu.c", True),
        ("linuxdoom-1.10/p_inter.c", True),
        ("linuxdoom-1.10/hu_lib.c", True),
        ("linuxdoom-1.10/d_items.c", False),
        ("tests/language_consumers.c", False),
    ):
        obj = temp / (Path(source).stem + ".o")
        command = [cc, "-std=c99", "-g", "-fsanitize=address,undefined", "-ffunction-sections", "-fdata-sections", "--param=asan-globals=0", "-fno-pie", "-I" + str(root / "linuxdoom-1.10"), "-I" + str(root / "win32")]
        if windows:
            command += ["-D_WIN32", "-DNORMALUNIX", "-DLINUX"]
        subprocess.run(command + ["-c", str(root / source), "-o", str(obj)], check=True)
        objects.append(str(obj))
    exe = temp / "consumer-test"
    subprocess.run([cc, "-no-pie", "-fsanitize=address,undefined", "-Wl,--gc-sections", *objects, "-o", str(exe)], check=True)
    game = temp / "game"
    (game / "languages").mkdir(parents=True)
    shutil.copyfile(root / "languages/es-ascii.ini", game / "languages/es-ascii.ini")
    for language in ("en", "es-ascii"):
        subprocess.run([str(exe), str(game), language], cwd=temp, check=True)
