"""Step S4.8 part 2, module audio_dev (log/roadmap/S4/8_notes.md): sound.dll's Ctrl objects, command queues, sound timer
clients, MIDI device exports, the sound thread and window, judged against the emulated original (decision G1).

The replay front end is port/test/sound_replay.py (`--step S4.8 --family ds|dev`; S4.8's log/roadmap/S4/8_replay.py until
S4.2b folded its goldens per module, custom keys, entry esp and import-log check into it). Default runs replay the
bundles (port/corpus/audio, local) against port/test/golden_audio_dev.json, x64 and Win32; SG_EMU_ORACLE=1 runs the live
oracle per key. Callees owned by other S4 steps (SoundTimer::registerClient / start, SoundWindowObj_ctor / dtor: S4.7)
and the static CRT's _atexit run through S4.2's development fallback; their counts are pinned in FALLBACKS.

    ~/bin/py -3.12 -m pytest port/test/test_audio_dev.py
    SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_dev.py -k oracle
"""
from __future__ import annotations

import os
import pathlib
import struct
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import sound_replay as R  # noqa: E402
from port.test import lib as L  # noqa: E402,F401

E = R.E

pytestmark = pytest.mark.port
FAMILY = "dev"
ORACLE = os.environ.get("SG_EMU_ORACLE") == "1"
KEYS = R.keys_for("S4.8", FAMILY)
SOUND_DLL = ROOT / "original" / "sound.dll"


# fallback calls per key (layout A bundles; the live oracle counts three layouts). With the whole S4 wave in one build (S4.7's
# SoundTimer / SoundWindowObj functions, S4.11's static CRT incl. _atexit) no key of this step makes a fallback call.
FALLBACKS: dict[str, int] = {}


def _functions():
    out = {}
    for k in KEYS:
        out.setdefault(R.port_name(k), []).append(k)
    return out


@pytest.fixture(scope="module")
def sndlib():
    p = R.mirror_dll()
    if not p.exists():
        pytest.skip(f"{p} not built (the S4 mirror, log/roadmap/S4/2b_mirror.py)")
    lib = R.load_lib()
    if "snd.CommandQueue::push" not in lib.functions():
        pytest.skip("the DLL has no audio_dev module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def sndlib32():
    from port.test import lib32 as L32
    try:
        lib = R.load_w32()
    except L32.RunnerUnavailable as e:
        pytest.skip(f"Win32 backend unavailable: {e}")
    if "snd.CommandQueue::push" not in lib.functions():
        pytest.skip("the Win32 DLL has no audio_dev module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def golden():
    g = R.load_golden("S4.8", FAMILY)
    if not g:
        pytest.skip("no golden digests (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.8 --family dev --update)")
    return g


def _replay(lib, key, golden):
    if R.load_bundle(key) is None:
        pytest.skip(f"no bundle for {key} (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.8 --family dev --update)")
    return R.replay_key(lib, key, golden)


# ================================================================================================ static
@pytest.mark.static
def test_every_row_has_a_key():
    rows = [r for r in R.step_rows("S4.8") if r["family"].rstrip("~") == FAMILY]
    assert len(rows) == 62
    assert {R.SND + r["name"] for r in rows} == set(_functions())


@pytest.mark.static
def test_goldens_cover_every_key():
    g = R.load_golden("S4.8", FAMILY)
    if not g:
        pytest.skip("no golden file")
    for k in KEYS:
        assert k in g, k
        assert g[k]["vectors"] == len(R.spec_of(k)["vectors"]) == len(g[k]["digests"]), k
        assert g[k]["tier"] in ("EXACT", "EQUAL_PI", "EQUAL_PI_DISTINCT_DIFF", "EMU_ONLY"), k
        assert g[k].get("fallbacks", 0) == 3 * FALLBACKS.get(k, 0), k


# ================================================================================================ default: bundles + goldens
@pytest.mark.parametrize("key", KEYS)
def test_key_equals_golden_x64(sndlib, golden, key):
    r = _replay(sndlib, key, golden)
    assert r.vectors and r.passed == r.vectors and r.fallbacks == FALLBACKS.get(key, 0), r


@pytest.mark.x87
def test_every_key_equals_golden_win32(sndlib32, golden):
    bad = []
    for key in KEYS:
        if R.load_bundle(key) is None:
            pytest.skip("bundles missing")
        r = R.replay_key(sndlib32, key, golden)
        if not (r.vectors and r.passed == r.vectors and r.fallbacks == FALLBACKS.get(key, 0)):
            bad.append((key, r.passed, r.vectors, r.first))
    assert not bad, bad


NEGCTL = [(fn, 1) for fn in sorted(_functions())]


@pytest.mark.parametrize("fn,mode", NEGCTL)
def test_negctl_red_against_golden(sndlib, golden, fn, mode):
    keys = _functions()[fn]
    for k in keys:
        if R.load_bundle(k) is None:
            pytest.skip("bundles missing")
    sndlib.negctl(fn, mode)
    try:
        bad = sum(r.vectors - r.passed for r in (R.replay_key(sndlib, k, golden) for k in keys))
    finally:
        sndlib.negctl(fn, 0)
    assert bad > 0, f"{fn}: mode {mode} reads GREEN on every vector"


def test_import_commands_equal_the_original_import_logs(sndlib, golden):
    """The port's import calls (the AudioSink import commands with no host) equal the emulated original's import stand-in
    logs record for record (sound_replay.sinklog_key)."""
    keys = [k for k in KEYS if "$implog" in [s[0] for s in R.spec_of(k).get("state", [])]]
    if not keys or any(R.load_bundle(k) is None for k in keys):
        pytest.skip("bundles missing")
    if any("implog" not in golden.get(k, {}) for k in keys):
        pytest.skip("goldens without the original's import logs (re-run the oracle with --update)")
    assert R.sinklog_main(keys) == 0


# ================================================================================================ the live oracle (opt-in)
@pytest.fixture(scope="module")
def vm():
    if not ORACLE:
        pytest.skip("SG_EMU_ORACLE=1 runs the emulator")
    with E.FixVM() as v:
        yield v


@pytest.mark.parametrize("key", KEYS)
def test_oracle_key(vm, sndlib, key):
    v, _, _ = R.oracle_key(vm, sndlib, key)
    assert v.match == v.vectors and v.match_bc == 2 * v.vectors and v.errors == 0, v
    assert v.fallbacks == 3 * FALLBACKS.get(key, 0), v
    g = R.load_golden("S4.8", FAMILY).get(key)
    if g:
        assert v.golden == g["digests"], "the emulated original no longer gives the pinned digests"


