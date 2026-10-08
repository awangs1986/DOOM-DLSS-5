"""Real profile files and separate processes through the public graphics seam."""
from pathlib import Path
import subprocess, tempfile, os
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="windoom-graphics-") as folder:
 p=Path(folder);exe=p/"settings"
 subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror","-g","-fsanitize=address,undefined","-fno-pie","-no-pie","-I"+str(root/"win32"),str(root/"win32/graphics_settings.c"),str(root/"tests/graphics_settings_test.c"),"-o",str(exe)],check=True)
 def run(mode,profile):subprocess.run([str(exe),mode,str(profile)],check=True,cwd=p)
 run("parse",p/"parse.cfg");run("basic",p/"a.cfg")
 # Saved a.cfg must not affect another -config identity in the same directory.
 run("defaults",p/"b.cfg");run("oversize",p/"oversize.cfg")
 (p/"directory.cfg.graphics.cfg").mkdir();run("refuse",p/"directory.cfg")
 (p/"reload.cfg.graphics.cfg").write_text("schema=1\nrt=1\nsr=0\nnr=1\n")
 run("reload",p/"reload.cfg")
 originals=list(p.glob("a.cfg.graphics.cfg.bak.*"));assert len(originals)==2
 assert any(x.read_text()=="schema=1\nrt=1\nsr=0\n" for x in originals)
 assert not list(p.glob("*.tmp.*"))
 print("PASS separate profiles/processes, bounded backups, no temporary debris")
