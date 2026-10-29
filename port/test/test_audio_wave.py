"""sound.dll's wave devices, wave-in capture, mmio RIFF readers, stream readers, decode rings and wave-file writers (step S4.9,
log/roadmap/S4/9_notes.md): module audio_wave, judged against the emulated original by port/test/sound_replay.py (`--step S4.9`;
S4.9's own runner log/roadmap/S4/9_replay.py until S4.2b). loadSoundFile, a row of S4.9, is S4.9b's module audio_soundfile
(test_audio_soundfile.py).

Default runs replay the recorded bundles (port/corpus/audio, local) and check the golden digests and x87 exit states pinned from the
emulator (port/test/golden_audio_wave.json) on x64 and Win32, and compare the port's import commands (the mmio, winmm and user32
imports, with no host) with the emulated original's import stand-in logs record for record. A fallback call is allowed only into
code no ported step owns (the static CRT, unnamed callees: S4.11's work), and the golden file pins which ("pending").
SG_EMU_ORACLE=1 runs the live oracle in three layouts, and the controls against it.

    ~/bin/py -3.12 -m pytest port/test/test_audio_wave.py
    SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_wave.py -k oracle

One test file per process (the whole port/test suite in one process exhausts memory).
"""
from __future__ import annotations

import os
import pathlib
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import lib as L  # noqa: E402,F401
from port.test import sound_replay as R  # noqa: E402

pytestmark = pytest.mark.port
STEP, MODULE, ROWS, PROBE = "S4.9", "audio_wave", 100, "snd.Wave_Device::ctor"
ORACLE = os.environ.get("SG_EMU_ORACLE") == "1"
KEYS = R.keys_for(STEP)


def _functions():
    out = {}
    for k in KEYS:
        out.setdefault(R.port_name(k), []).append(k)
    return out


@pytest.fixture(scope="module")
def sndlib():
    lib = R.load_lib()
    if PROBE not in lib.functions():
        pytest.skip(f"the DLL has no {MODULE} module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def sndlib32():
    from port.test import lib32 as L32
    try:
        lib = R.load_w32()
    except (L32.RunnerUnavailable, FileNotFoundError, OSError) as e:
        pytest.skip(f"Win32 backend unavailable: {e}")
    if PROBE not in lib.functions():
        pytest.skip(f"the Win32 DLL has no {MODULE} module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def golden():
    g = R.load_golden(STEP)
    if not g:
        pytest.skip(f"no golden digests (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step {STEP} --update)")
    return g


# ================================================================================================ static
@pytest.mark.static
def test_every_row_has_a_key():
    rows = [r for r in R.step_rows(STEP) if r["name"] not in R.STEP_OF_FUNCTION]
    assert len(rows) == ROWS
    assert {r["name"] for r in rows} == {R.row_name(k) for k in KEYS}
    assert len(KEYS) == 247


@pytest.mark.static
def test_goldens_cover_every_key():
    g = R.load_golden(STEP)
    if not g:
        pytest.skip("no golden file")
    for k in KEYS:
        assert k in g, k
        assert g[k]["vectors"] == len(R.spec_of(k)["vectors"]) == len(g[k]["digests"]) == len(g[k]["x87_exit"]), k
        assert g[k]["tier"] in R.TIERS, k
        assert not g[k].get("fallbacks") or g[k].get("pending"), k   # fallbacks only into code no ported step owns


@pytest.mark.static
def test_module_source_rules():
    src = (ROOT / "port" / "src" / "sim" / f"{MODULE}.cpp").read_text(encoding="utf-8")
    assert "TODO(convert)" not in src
    assert src.count("neg_u32(") + src.count("neg_mem(") >= 90
    # the core's stack frame is the only stack-locals mechanism (S4.2b): no module helper of its own
    assert "SndFrame F(" in src and "WaveFrame" not in src and "SndLocals" not in src


# ================================================================================================ the module in the library
def test_module_registers_every_row(sndlib):
    have = set(sndlib.functions())
    names = {"snd." + r["name"] for r in R.step_rows(STEP) if r["name"] not in R.STEP_OF_FUNCTION}
    assert names <= have, sorted(names - have)


def _replay(lib, key, golden):
    if R.load_bundle(key) is None:
        pytest.skip(f"no bundle for {key} (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step {STEP} --update)")
    return R.replay_key(lib, key, golden)


@pytest.mark.parametrize("key", KEYS)
def test_key_equals_golden_x64(sndlib, golden, key):
    r = _replay(sndlib, key, golden)
    assert R.replay_ok(r), r


@pytest.mark.x87
def test_every_key_equals_golden_win32(sndlib32, golden):
    bad = []
    for key in KEYS:
        r = _replay(sndlib32, key, golden)
        if not R.replay_ok(r):
            bad.append((key, r.passed, r.vectors, r.first))
    assert not bad, bad


@pytest.mark.parametrize("fn", sorted(_functions()))
def test_negctl_red_against_golden(sndlib, golden, fn):
    """Mode 1 of every function: at least one vector of its keys no longer hashes to the emulator's digest."""
    keys = _functions()[fn]
    sndlib.negctl(fn, 1)
    try:
        bad = sum(r.vectors - r.passed for r in (_replay(sndlib, k, golden) for k in keys))
    finally:
        sndlib.negctl(fn, 0)
    assert bad > 0, f"{fn}: mode 1 reads GREEN on every vector"


def test_import_commands_equal_the_original_import_logs(sndlib, golden):
    """The port's import calls (AudioSink import commands with no host) equal the emulated original's import stand-in logs, record
    for record (sound_replay sinklog: 83 keys carry a `$implog` region)."""
    keys = [k for k in KEYS if "$implog" in [s[0] for s in R.spec_of(k).get("state", [])]]
    assert len(keys) == 83
    if any(R.load_bundle(k) is None for k in keys):
        pytest.skip("bundles missing")
    if any("implog" not in golden.get(k, {}) for k in keys):
        pytest.skip("goldens without the original's import logs (re-run the oracle with --update)")
    imps = R.sound_imports()
    bad = [(k, v, first) for k in keys for v, _, _, _, _, first in [R.sinklog_key(sndlib, k, golden, imps)] if v == "DIFF"]
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


@pytest.mark.parametrize("fn", sorted(_functions()))
def test_oracle_negctl_red(vm, sndlib, fn):
    sndlib.negctl(fn, 1)
    try:
        bad = 0
        for k in _functions()[fn]:
            v, _, _ = R.oracle_key(vm, sndlib, k, ("A",), record=False)
            bad += v.vectors - v.match
    finally:
        sndlib.negctl(fn, 0)
    assert bad > 0
