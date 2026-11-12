"""P24D (log/roadmap/P24/P24D_notes.md): the language pack's matcher (port/src/sim/text.cpp, lang section) through the app's --selftest-p24d,
on a synthetic pack (no game text): exact lines, misses, trailing blanks, printf patterns with a translated %s capture, %%, escapes,
code page 1252, English unchanged, and the refusal of a line whose conversions differ. The control `--negctl p24d-mismatch` swaps
one pattern's conversions, so its load count, refusal and pattern rows read FAIL. Plus scripts/lang/check_pack.py's refusals.
P24J: %s parts are translated three levels deep (a comment tip's tile); `--negctl p24j-depth2` (the old depth) reads those rows FAIL."""
from __future__ import annotations

import os
import pathlib
import subprocess
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def app_exe() -> pathlib.Path:
    env = os.environ.get("SG_APP_EXE")
    cands = ([pathlib.Path(env)] if env else []) + [ROOT / "port" / "build" / "x64" / "app" / "Release" / "simgolf_port.exe"]
    exe = next((c for c in cands if c.exists()), None)
    if exe is None:
        pytest.skip("no simgolf_port.exe")
    return exe


def rows(args: list[str]) -> dict[str, bool]:
    r = subprocess.run([str(app_exe())] + args, capture_output=True, text=True, timeout=120)
    out = {}
    for line in r.stdout.splitlines():
        if line.startswith("[") and "] " in line:
            name, status = line.split("] ", 1)[1].rsplit(" ", 1)
            out[name] = status == "PASS"
    return out


def test_selftest_p24d_all_pass():
    r = rows(["--selftest-p24d"])
    assert len(r) == 16 and all(r.values()), r  # P24J: + 4 capture-depth rows


def test_selftest_p24j_depth_control_reads_red():
    # P24J: at the old capture depth (2) the tile three levels down stays English
    r = rows(["--selftest-p24d", "--negctl", "p24j-depth2"])
    failed = {k for k, v in r.items() if not v}
    assert failed == {"p24j-depth3-tile-translated", "p24j-depth-restored"}, r


def test_selftest_p24d_control_reads_red():
    r = rows(["--selftest-p24d", "--negctl", "p24d-mismatch"])
    failed = {k for k, v in r.items() if not v}
    assert failed == {"p24d-load-counts", "p24d-refuses-conversion-mismatch", "p24d-pattern-number-text"}, r


def test_check_pack_refuses(tmp_path):
    pack = tmp_path / "pack.txt"
    pack.write_text("Hole %d\tHoyo %d\nHole %d of %s\tHoyo %s de %d\nOne\tUno\nOne\tUna\n50%\t50%\n", encoding="utf-8")
    r = subprocess.run([sys.executable, str(ROOT / "scripts" / "lang" / "check_pack.py"), str(pack)], capture_output=True,
                       text=True, timeout=60)
    assert r.returncode == 1
    assert "line 2: conversions differ" in r.stdout and "line 4: English side already on line 3" in r.stdout, r.stdout
    assert "line 5: a '%' that starts no conversion" in r.stdout, r.stdout
