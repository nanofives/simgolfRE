"""INT3 coverage (shim/src/coverage.cpp): arms every Ghidra function entry, records per-phase hits."""
import pathlib
import tempfile
import time

import pytest

import imgcmp
from conftest import GOLDEN, ROOT
from game import Game

pytestmark = pytest.mark.runtime


def test_coverage_boot_to_menu(tmp_path):
    rows = (ROOT / "re" / "functions_ghidra.tsv").read_text().splitlines()[1:]
    entries = tmp_path / "entries.txt"
    entries.write_text("".join(f"golf_clean.exe\t{r.split(chr(9))[0]}\n" for r in rows))
    dump = tmp_path / "cov.tsv"
    if Game.running_instances():
        pytest.fail("SimGolf already running")
    with Game(env={"SIMGOLF_SKIP_INTRO": "1"}, coverage=entries) as g:
        g.wait_window()
        imgcmp.wait_for(g, imgcmp.load(GOLDEN / "main_menu.png"), timeout=60)  # coverage must not break boot
        assert g.cov_phase(1) > 2000                                            # re-arm works
        time.sleep(2)
        n = g.cov_dump(dump)
    hits = {l.split("\t")[1]: int(l.split("\t")[2], 16) for l in dump.read_text().splitlines()[1:]}
    assert n == len(hits) > 400
    assert hits["004a682f"] & 1            # PE entry ran during boot (phase 0)
    assert hits["0045baf0"] & 1            # WinMain
    assert "004490d0" not in hits          # dead Terrain::tileAt copy never runs
