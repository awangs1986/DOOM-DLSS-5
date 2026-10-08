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

    (packs / "templated.ini").write_text("[pack]\nschema=1\n[strings]\nquicksave.prompt=Guardar '{save_name}'?\nquickload.prompt=Cargar '{save_name}'?\nprompt.yes_no=Y o N\n", encoding="utf-8")
    (packs / "badtemplate.ini").write_text("[pack]\nschema=1\n[strings]\nquicksave.prompt=must not leak {unknown}\nquickload.prompt={save_name} {save_name}\n", encoding="utf-8")
    (packs / "braces.ini").write_text("[pack]\nschema=1\n[strings]\nquicksave.prompt={{save}} {save_name} 100%\n", encoding="utf-8")

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
    message = output.split("MESSAGE=", 1)[1].split("SAVE=", 1)[0].rstrip("\n")
    assert len(message.splitlines()) <= 8
    assert all(len(line) <= 30 for line in message.splitlines())
    assert message.endswith("(press y to quit)")
    output = run("templated")
    assert "SAVE=Guardar 'MY SAVE%N{literal}'?" in output
    assert "LOAD=Cargar 'MY SAVE%N{literal}'?" in output
    assert "SAVE_DIAG=\n" in output and "LOAD_DIAG=\n" in output
    assert "Y o N" in output
    output = run("badtemplate")
    assert "must not leak" not in output
    assert "quicksave over your game named" in output
    assert "do you want to quickload the game named" in output
    assert "invalid translated template; using English template" in output
    output = run("braces")
    assert "SAVE={save} MY SAVE%N{literal} 100%" in output
    (game / "language.cfg").write_text("../es\n")
    assert "SELECTED=en" in run()
print("language startup/persistence/fallback tests passed")
