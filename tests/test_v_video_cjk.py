"""Compile the real software glyph blitter and check exact G-buffer mask runs."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
cc = os.environ.get("CC", "cc")
with tempfile.TemporaryDirectory(prefix="windoom-cjk-blit-") as tmp:
    exe = Path(tmp) / "blit-test"
    subprocess.run([
        cc, "-std=c99", "-g", "-fsanitize=address,undefined",
        "-ffunction-sections", "-fdata-sections", "--param=asan-globals=0",
        "-fno-pie", "-D_WIN32", "-DNORMALUNIX", "-DLINUX",
        "-I" + str(root / "linuxdoom-1.10"), "-I" + str(root / "win32"),
        str(root / "linuxdoom-1.10/v_video.c"),
        str(root / "tests/v_video_cjk_test.c"), "-no-pie", "-Wl,--gc-sections",
        "-fsanitize=address,undefined", "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("CJK glyph pixel/overlay blit tests passed")
