"""The sound.dll channel and voice family (step S4.6, log/roadmap/S4/6_notes.md): module audio_chan, judged against the emulated
original by port/test/sound_replay.py (`--step S4.6`; S4.6's own runner log/roadmap/S4/6_run.py until S4.2b).

Default runs replay the recorded bundles (port/corpus/audio, local) and check the golden digests and x87 exit states pinned from
the emulator (port/test/golden_audio_chan.json), x64 and Win32: since S4.2b the c3fh caller-word keys (x87 control word 0x0f7f)
run on the core's x87 unit on both backends, so x64 judges them too. SG_EMU_ORACLE=1 runs the live oracle (Node fixture VM +
Unicorn over the user's sound.dll) in three layouts, and the negative controls against it.

    ~/bin/py -3.12 -m pytest port/test/test_audio_chan.py
    SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_chan.py -k oracle

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
STEP = "S4.6"
ORACLE = os.environ.get("SG_EMU_ORACLE") == "1"
KEYS = R.keys_for(STEP)
X87 = tuple(R.SND + n for n in ("Channel_setRate", "Channel_recomputeVoiceGains", "Channel_setReverseRate", "Channel_stepVolumeFade",
                                 "Voice_resetDefaults"))


def _functions():
    out = {}
    for k in KEYS:
        out.setdefault(R.port_name(k), []).append(k)
    return out


@pytest.fixture(scope="module")
def chan():
    lib = R.load_lib()
    if "snd.Channel_setPanGains" not in lib.functions():
        pytest.skip("the DLL has no audio_chan module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def chan32():
    from port.test import lib32 as L32
    try:
        lib = R.load_w32()
    except (L32.RunnerUnavailable, FileNotFoundError, OSError) as e:
        pytest.skip(f"Win32 backend unavailable: {e}")
    if "snd.Channel_setPanGains" not in lib.functions():
        pytest.skip("the Win32 DLL has no audio_chan module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def golden():
    g = R.load_golden(STEP)
    if not g:
        pytest.skip("no golden digests (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.6 --update)")
    return g


# ================================================================================================ static
@pytest.mark.static
def test_every_step_row_has_a_key_or_a_derived_key():
    rows = R.step_rows(STEP)
    names = {R.port_name(k) for k in KEYS}
    assert len(rows) == 101
    assert {R.SND + r["name"] for r in rows} == names


@pytest.mark.static
def test_derived_keys_borrow_a_fixture_with_the_same_argument_slots():
    # the emu-only rows: their own address on a twin's fixture (the twin's vectors carry the slots the function pops)
    for key, slots in (("stubReturnBC@", 2), ("Channel_stubReturnB@", 4), ("VoiceSlot_dtorEmpty@", 1)):
        assert key in KEYS
        spec = R.spec_of(key)
        assert spec["abi"] == "thiscall" and len(spec["vectors"][0]) >= slots - 1


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
        for k in _functions()[fn]:
            words.update(g[k].get("fpcw", []))
        assert 0x0F7F in words and R.DEFAULT_CW in words, (fn, words)


@pytest.mark.static
def test_module_source_has_no_leftover_conversion_marks():
    src = (ROOT / "port" / "src" / "sim" / "audio_chan.cpp").read_text()
    assert "TODO(convert)" not in src and "frame_cell" not in src and "__asm" not in src
    assert src.count("neg_u32(") + src.count("neg_mem(") >= 100


# ================================================================================================ the module in the library
def test_module_registers_every_ported_row(chan):
    have = set(chan.functions())
    names = {R.SND + r["name"] for r in R.step_rows(STEP)}
    assert names <= have, sorted(names - have)


def _replay(lib, key, golden):
    if R.load_bundle(key) is None:
        pytest.skip(f"no bundle for {key} (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.6 --update)")
    return R.replay_key(lib, key, golden)


@pytest.mark.parametrize("key", KEYS)
def test_key_equals_golden_x64(chan, golden, key):
    r = _replay(chan, key, golden)
    assert R.replay_ok(r), r


@pytest.mark.x87
def test_every_key_equals_golden_win32(chan32, golden):
    bad, judged = [], 0
    for key in KEYS:
        r = _replay(chan32, key, golden)
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
def test_negctl_red_against_golden(chan, golden, fn):
    """Mode 1 of every function: at least one vector of its keys no longer hashes to the emulator's digest."""
    assert _bad(chan, golden, fn, 1, _functions()[fn]) > 0, f"{fn}: mode 1 reads GREEN on every vector"


@pytest.mark.parametrize("fn", list(X87))
def test_wrong_x87_port_is_red_at_word_0f7f(chan, golden, fn):
    """Mode 2 (plain double quotients, the classic wrong x87 port) reads RED on the vectors the 0x0f7f word changes and equal
    at 0x027f (where 53-bit doubles rounded to nearest are the unit's own arithmetic: an equivalent mutant there, recorded).
    On the x87 unit this holds on x64 too (S4.2b)."""
    keys = _functions()[fn]
    wf7f = _bad(chan, golden, fn, 2, [k for k in keys if k.endswith("_wf7f")])
    w27f = _bad(chan, golden, fn, 2, [k for k in keys if not k.endswith("_wf7f")])
    assert wf7f > 0, f"{fn}: the wrong port reads GREEN at 0x0f7f"
    assert w27f == 0


@pytest.mark.parametrize("mode", range(2, 9))
def test_voice_update_modes_red_against_golden(chan, golden, mode):
    """Voice::update's modes 2..8 (hand_update.cpp: X == [this+0x720], the round limit, `found`, the registry count, the +0x4c
    update, a NaN in [this+0x728], the unlink bit)."""
    assert _bad(chan, golden, "snd.Voice::update", mode, _functions()["snd.Voice::update"]) > 0


# ================================================================================================ the live oracle (opt-in)
@pytest.fixture(scope="module")
def vm():
    if not ORACLE:
        pytest.skip("SG_EMU_ORACLE=1 runs the emulator")
    with R.E.FixVM() as v:
        yield v


@pytest.mark.parametrize("key", KEYS)
def test_oracle_key(vm, chan, key):
    v, _, _ = R.oracle_key(vm, chan, key)
    assert v.verdict in ("GREEN", "GREEN_PENDING") and v.errors == 0, v
    g = R.load_golden(STEP).get(key)
    if g:
        assert v.golden == g["digests"], "the emulated original no longer gives the pinned digests"


@pytest.mark.parametrize("fn", sorted(_functions()))
def test_oracle_negctl_red(vm, chan, fn):
    chan.negctl(fn, 1)
    try:
        bad = 0
        for k in _functions()[fn]:
            v, _, _ = R.oracle_key(vm, chan, k, ("A",), record=False)
            bad += v.vectors - v.match
    finally:
        chan.negctl(fn, 0)
    assert bad > 0
