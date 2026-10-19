"""Phase 2.5 vector corpus end to end on the real game: capture -> build -> offline replay, for two small functions.

Boots instances\\p25 once (SIMGOLF_HOOKS_OFF=all) and captures the registry keys `clamp` (a pure leaf) and
`Random::range` (it reads and advances the game RNG through Random::next) into a temporary corpus, builds their .vec
files and replays every vector in Unicorn from the .vec alone. Every vector must reproduce: the live original's return
value, and the bytes the emulated original writes (Random::range's RNG state) equal to what the live call changed.
"""
import csv
import os
import subprocess
import sys

import pytest

from conftest import ROOT

pytestmark = pytest.mark.runtime

TOOLS = ROOT / "re" / "tools"


def run(args, env, timeout=600):
    r = subprocess.run([sys.executable, *args], capture_output=True, text=True, timeout=timeout, cwd=str(ROOT), env=env)
    return r.returncode, r.stdout + r.stderr


def test_capture_build_replay(tmp_path):
    if not (ROOT / "instances" / "p25" / "golf_clean.exe").exists():
        pytest.skip("instances\\p25 missing (re/tools/instances.py create p25)")
    env = dict(os.environ, SIMGOLF_VEC_DIR=str(tmp_path / "vectors"), SIMGOLF_VEC_MANIFEST=str(tmp_path / "manifest.tsv"),
               SIMGOLF_VEC_REPORT_DIR=str(tmp_path / "report"), SIMGOLF_ALLOW_MULTI="1",
               SIMGOLF_GAME_DIR=str(ROOT / "instances" / "p25"))
    rc, log = run([str(TOOLS / "vector_capture.py"), "capture", "--key", "clamp", "Random::range"], env)
    assert rc == 0, log[-3000:]
    assert (tmp_path / "vectors" / "raw" / "clamp.raw.gz").exists(), log[-3000:]
    rc, log = run([str(TOOLS / "vector_capture.py"), "build", "--function", "clamp", "Random::range"], env)
    assert rc == 0, log[-3000:]
    rows = {r["function"]: r for r in csv.DictReader((tmp_path / "manifest.tsv").open(), delimiter="\t")}
    for fn in ("clamp", "Random::range"):
        r = rows[fn]
        assert r["status"] == "ok" and int(r["ok_vectors"]) == int(r["vectors"]) >= 10, r
        assert int(r["distinct_results"]) > 1, r
    assert rows["Random::range"]["x87"] == "1"          # Random::next's double arithmetic runs under it
    rc, log = run([str(TOOLS / "vector_validate.py"), "emu", "--function", "clamp", "Random::range"], env)
    assert rc == 0, log[-3000:]
    val = list(csv.DictReader((tmp_path / "report" / "validate_emu.tsv").open(), delimiter="\t"))
    assert {v["function"] for v in val} == {"clamp", "Random::range"}
    for v in val:
        assert int(v["fail"]) == 0 and int(v["pass"]) >= 10, v
