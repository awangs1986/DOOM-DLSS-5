"""Real files + subprocesses check startup behavior through the public consumer."""
from pathlib import Path
import subprocess
import sys
import tempfile

exe = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="windoom-language-") as tmp:
    root = Path(tmp)
    game = root / "game"
    packs = game / "languages"
    packs.mkdir(parents=True)
    unrelated = root / "unrelated"
    unrelated.mkdir()
    (packs / "es.ini").write_text("[pack]\nschema=1\n[strings]\nquit.prompt=Salir?\n", encoding="utf-8")
    (packs / "bad.ini").write_text("[pack]\nschema=1\n[strings]\nquit.prompt=must not leak\nfault=bad\\q\n", encoding="utf-8")
    (packs / "huge.ini").write_text("[pack]\nschema=1\n[strings]\nquit.prompt=" + "x" * 60000, encoding="utf-8")
    (packs / "unicode.ini").write_text("[pack]\nschema=1\n[strings]\nquit.prompt=退出?\nquit.confirm=Y\n", encoding="utf-8")
    (packs / "long.ini").write_text("[pack]\nschema=1\n[strings]\nquit.prompt=" + "A" * 1000 + "\n", encoding="utf-8")

    def run(selection=None):
        command = [str(exe), str(game)]
        if selection is not None:
            command.append(selection)
        return subprocess.run(command, cwd=unrelated, check=True, capture_output=True, text=True).stdout

    assert "SELECTED=en" in run()
    output = run("es")
    assert "SELECTED=es" in output and "PROMPT=Salir?" in output
    assert "CONFIRM=(press y to quit)" in output  # only missing key falls back
    assert (game / "language.cfg").read_text() == "es\n"
    assert "SELECTED=es" in run()  # actual persisted selection, no CLI
    assert "SELECTED=en" in run("en")
    assert "SELECTED=en" in run()
    run("es")
    for selection in ("bad", "huge", "missing", "../bad", ""):
        output = run(selection)
        assert "SELECTED=en" in output and "using en" in output
        assert "must not leak" not in output  # valid prefix is never published
        assert (game / "language.cfg").read_text() == "es\n"  # failure preserves preference
    output = run("unicode")
    assert "PROMPT=退出?" in output and "MESSAGE=???\n\nY" in output
    output = run("long")
    message = output.split("MESSAGE=", 1)[1].rstrip("\n")
    assert len(message.splitlines()) <= 8
    assert all(len(line) <= 30 for line in message.splitlines())
    assert message.endswith("(press y to quit)")
    (game / "language.cfg").write_text("../es\n")
    assert "SELECTED=en" in run()
print("language startup/persistence/fallback tests passed")
