"""sound.dll's sound-bank loader loadSoundFile (rva 0x41d0, slot 4 of the Sound vtable; step S4.9b, log/roadmap/S4/9b_notes.md):
module audio_soundfile, registered as `snd.loadSoundFile`, judged against the emulated original (decision G1: no hook source, no
path-1 row) by port/test/sound_replay.py (`--step S4.9b`; S4.9b's own runner log/roadmap/S4/9b_replay.py until S4.2b). Its keys
run over port/test/sound_fixtures/s49b_soundfile.js: synthetic bank files built per layout and the user's own files from original/.

Default runs replay the bundles (port/corpus/audio, local) against port/test/golden_audio_soundfile.json on x64 and Win32, run
every negative-control mode against the goldens and compare the port's mmio import commands with the original's import logs.
Its callees (the static CRT, the four of U-1493) are S4.11's port functions (audio_crt), so no key makes a fallback call.
SG_EMU_ORACLE=1 adds the live oracle per key.

    ~/bin/py -3.12 -m pytest port/test/test_audio_soundfile.py
    SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_soundfile.py -k oracle

One test file per process (the whole port/test suite in one process exhausts memory).
"""
from __future__ import annotations

import csv
import os
import pathlib
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import sound_replay as R  # noqa: E402

pytestmark = pytest.mark.port
STEP = "S4.9b"
ORACLE = os.environ.get("SG_EMU_ORACLE") == "1"
KEYS = R.keys_for(STEP)
FN = "snd.loadSoundFile"
NEG_MODES = range(1, 11)          # audio_soundfile.cpp kNegReturn .. kNegUnknown
# conditional jumps of the original no fixture state reaches on one side (9b_notes.md section 4: the list drains of the two exits
# before the chunk loop run on an always-empty list, +0x948's address is never NULL, a NULL MixBuffer faults the original first,
# the +0x220 cursor is never NULL inside the record loop)
ONE_SIDED = {"0x428d", "0x4296", "0x42bf", "0x42cc", "0x4356", "0x435f", "0x4388", "0x4395", "0x4627", "0x481f", "0x49cd", "0x4ad5",
             "0x4fad"}


@pytest.fixture(scope="module")
def sndlib():
    lib = R.load_lib()
    if FN not in lib.functions():
        pytest.skip("the DLL has no audio_soundfile module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def sndlib32():
    from port.test import lib32 as L32
    try:
        lib = R.load_w32()
    except (L32.RunnerUnavailable, FileNotFoundError, OSError) as e:
        pytest.skip(f"Win32 backend unavailable: {e}")
    if FN not in lib.functions():
        pytest.skip("the Win32 DLL has no audio_soundfile module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def golden():
    g = R.load_golden(STEP)
    if not g:
        pytest.skip(f"no golden digests (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step {STEP} --update)")
    return g


def _need_bundles():
    for k in KEYS:
        if R.load_bundle(k) is None:
            pytest.skip(f"no bundle for {k} (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step {STEP} --update)")


# ================================================================================================ static
@pytest.mark.static
def test_row_is_this_step_and_named_by_the_rule():
    rows = [r for r in R.step_rows_all() if r["name"] == "loadSoundFile"]
    assert len(rows) == 1 and int(rows[0]["rva"], 16) == 0x41d0
    assert R.STEP_OF_FUNCTION["loadSoundFile"] == STEP
    assert {R.port_name(k) for k in KEYS} == {FN}
    src = (ROOT / "port" / "src" / "sim" / "audio_soundfile.cpp").read_text(encoding="utf-8")
    assert '{"snd.loadSoundFile", "thiscall", "uint32", 2,' in src
    # the core's stack frame is the only stack-locals mechanism (S4.2b)
    assert "SndFrame F;" in src and "SfFrame" not in src and "SndLocals" not in src


@pytest.mark.static
def test_goldens_cover_every_key():
    g = R.load_golden(STEP)
    if not g:
        pytest.skip("no golden file")
    assert len(KEYS) == 25
    for k in KEYS:
        assert k in g, k
        assert g[k]["vectors"] == len(R.spec_of(k)["vectors"]) == len(g[k]["digests"]) == len(g[k]["x87_exit"]), k
        assert g[k]["tier"] == "EMU_ONLY", k
        assert not g[k].get("fallbacks") or g[k].get("pending"), k   # fallbacks only into code no ported step owns
        assert "implog" in g[k], k
    # every exit of the original is a pinned result: 0, 7 (the +0x14 slot's answer), 8, 0x12
    assert sum(g[k]["vectors"] for k in KEYS) == 74


@pytest.mark.static
def test_recorded_coverage_leaves_only_the_dead_sides():
    p = ROOT / "log" / "roadmap" / "S4" / "9b_coverage.tsv"
    if not p.exists():
        pytest.skip("no coverage table (log/roadmap/S4/9b_cover.py)")
    rows = []
    with p.open(newline="") as f:
        for r in csv.reader(f, delimiter="\t"):
            if not r:
                break
            rows.append(r)
    jcc = {r[0]: r[3] for r in rows[1:]}
    assert len(jcc) == 101
    assert {k for k, v in jcc.items() if v != "both"} == ONE_SIDED


# ================================================================================================ the module in the library
def test_module_registers_the_function(sndlib):
    assert FN in set(sndlib.functions())


@pytest.mark.parametrize("key", KEYS)
def test_key_equals_golden_x64(sndlib, golden, key):
    _need_bundles()
    r = R.replay_key(sndlib, key, golden)
    assert R.replay_ok(r), r


@pytest.mark.x87
def test_every_key_equals_golden_win32(sndlib32, golden):
    _need_bundles()
    bad = []
    for key in KEYS:
        r = R.replay_key(sndlib32, key, golden)
        if not R.replay_ok(r):
            bad.append((key, r.passed, r.vectors, r.first))
    assert not bad, bad


@pytest.mark.parametrize("mode", NEG_MODES)
def test_negctl_red_against_golden(sndlib, golden, mode):
    """Every mode (one wrong detail per chunk handler, the prefix and the unknown-tag exit): at least one vector no longer hashes to
    the emulated original's digest."""
    _need_bundles()
    sndlib.negctl(FN, mode)
    try:
        bad = sum(r.vectors - r.passed for r in (R.replay_key(sndlib, k, golden) for k in KEYS))
    finally:
        sndlib.negctl(FN, 0)
    assert bad > 0, f"mode {mode} reads GREEN on every vector"


def test_import_commands_equal_the_original_import_logs(sndlib, golden):
    """The port's mmio import commands equal the emulated original's mmio stand-in logs record for record (sound_replay sinklog)."""
    _need_bundles()
    imps = R.sound_imports()
    bad = [(k, v, first) for k in KEYS for v, _, _, _, _, first in [R.sinklog_key(sndlib, k, golden, imps)] if v != "EQUAL"]
    assert not bad, bad


# ================================================================================================ the live oracle (opt-in)
@pytest.fixture(scope="module")
def vm():
    if not ORACLE:
        pytest.skip("SG_EMU_ORACLE=1 runs the emulator")
    with R.E.FixVM() as v:
        yield v


@pytest.mark.parametrize("key", KEYS)
def test_oracle_key(vm, sndlib, key):
    v, _, _ = R.oracle_key(vm, sndlib, key)
    assert v.verdict in ("GREEN", "GREEN_PENDING") and v.errors == 0, v
    g = R.load_golden(STEP).get(key)
    if g and not v.forgiven:
        assert v.golden == g["digests"], "the emulated original no longer gives the pinned digests"
