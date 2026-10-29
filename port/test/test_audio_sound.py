"""The sound.dll sound objects (step S4.7, log/roadmap/S4/7_notes.md): module audio_sound, judged against the emulated original by
port/test/sound_replay.py (`--step S4.7`; S4.7's own runner log/roadmap/S4/7_run.py until S4.2b). getPlaybackTime's keys run on
the fixture VM with a constant WINMM clock (sound_replay CLOCK_FUNCTIONS, U-1462).

Default runs replay the recorded bundles (port/corpus/audio, local) and check the golden digests and x87 exit states pinned from the
emulator (port/test/golden_audio_sound.json), x64 and Win32: since S4.2b Sound5b6a8_ctor's caller-word keys (x87 control word 0x0f7f)
run on the core's x87 unit on both backends. SG_EMU_ORACLE=1 runs the live oracle in three layouts, and the controls against it.

    ~/bin/py -3.12 -m pytest port/test/test_audio_sound.py
    SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_sound.py -k oracle

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
STEP, MODULE, ROWS, PROBE = "S4.7", "audio_sound", 66, "snd.Sound5b6a8_ctor"
ORACLE = os.environ.get("SG_EMU_ORACLE") == "1"
KEYS = R.keys_for(STEP)
X87 = ("Sound5b6a8_ctor",)


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
def test_every_step_row_has_a_key():
    rows = R.step_rows(STEP)
    assert len(rows) == ROWS
    assert {r["name"] for r in rows} == {R.row_name(k) for k in KEYS}


@pytest.mark.static
def test_goldens_cover_every_key():
    g = R.load_golden(STEP)
    if not g:
        pytest.skip("no golden file")
    for k in KEYS:
        assert k in g, k
        assert g[k]["vectors"] == len(R.spec_of(k)["vectors"]) == len(g[k]["digests"]) == len(g[k]["x87_exit"]), k
        assert g[k]["tier"] in ("EXACT", "EQUAL_PI", "EQUAL_PI_DISTINCT_DIFF", "EMU_ONLY"), k


@pytest.mark.static
def test_x87_functions_have_word_0f7f_vectors():
    g = R.load_golden(STEP)
    if not g:
        pytest.skip("no golden file")
    for fn in X87:
        words = set()
        for k in _functions()["snd." + fn]:
            words.update(g[k].get("fpcw", []))
        assert 0x0F7F in words and R.DEFAULT_CW in words, (fn, words)


@pytest.mark.static
def test_module_source_rules():
    src = (ROOT / "port" / "src" / "sim" / f"{MODULE}.cpp").read_text()
    assert "TODO(convert)" not in src and "fcell(" not in src and "__asm" not in src
    assert src.count("neg_u32(") + src.count("neg_mem(") >= ROWS


# ================================================================================================ the module in the library
def test_module_registers_every_row(sndlib):
    have = set(sndlib.functions())
    names = {"snd." + r["name"] for r in R.step_rows(STEP)}
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
    bad, judged = [], 0
    for key in KEYS:
        r = _replay(sndlib32, key, golden)
        judged += r.vectors
        if not R.replay_ok(r):
            bad.append((key, r.passed, r.vectors, r.first))
    assert not bad, bad
    assert judged == sum(g["vectors"] for k, g in golden.items() if k in KEYS)


def _bad(lib, golden, fn, mode, keys):
    lib.negctl(fn, mode)
    try:
        return sum(r.vectors - r.passed for r in (_replay(lib, k, golden) for k in keys))
    finally:
        lib.negctl(fn, 0)


@pytest.mark.parametrize("fn", sorted(_functions()))
def test_negctl_red_against_golden(sndlib, golden, fn):
    assert _bad(sndlib, golden, fn, 1, _functions()[fn]) > 0, f"{fn}: mode 1 reads GREEN on every vector"


@pytest.mark.parametrize("fn", X87)
def test_wrong_x87_port_is_an_equivalent_mutant_on_these_vectors(sndlib, golden, fn):
    """Mode 2 (plain doubles) of Sound5b6a8_ctor is BLIND on the C3 vectors at both words (U-1461): the constructor computes its
    tempo seed from constants on freshly cleared fields, so no vector input reaches the arithmetic. The correct body and the
    mutant both equal the goldens; if a vector that separates them is ever added the assertion fails and the control is live."""
    keys = _functions()["snd." + fn]
    assert _bad(sndlib, golden, "snd." + fn, 0, keys) == 0
    assert _bad(sndlib, golden, "snd." + fn, 2, keys) == 0, f"{fn}: mode 2 now differs: the control is live, update U-1461"


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
