"""sound.dll's LPC-10 speech codec (step S4.3, log/roadmap/S4/3_notes.md): module audio_codec, judged against the emulated
original by port/test/sound_replay.py (`--step S4.3`; S4.3's own runner log/roadmap/S4/3_run.py until S4.2b).

Default runs replay the recorded bundles (port/corpus/audio, local) and check the golden digests pinned from the emulator
(port/test/golden_audio_codec.json) on x64 and on the Win32 backend. Every key ran at the x87 word 0x027f, where the port's
double arithmetic with the original's float spills is exact on both backends (U-1421 for other words). The codec's float
compares are C comparisons: its bodies mark the x87 unit's condition codes unknown, and the harness compares the rest of the
x87 exit state (S4.2b, U-1419). SG_EMU_ORACLE=1 runs the live oracle in three layouts, and the negative controls against it.

    ~/bin/py -3.12 -m pytest port/test/test_audio_codec.py
    SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_codec.py -k oracle

One test file per process (the whole port/test suite in one process exhausts memory).
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

from port.test import lib as L  # noqa: E402
from port.test import sound_replay as R  # noqa: E402

pytestmark = pytest.mark.port
STEP = "S4.3"
ORACLE = os.environ.get("SG_EMU_ORACLE") == "1"
KEYS = R.keys_for(STEP)
# mode 2: a body that skips one `fstp dword` spill (keeps the double); each must read RED on its own keys
SPILL_CONTROLS = ("codecRemoveMean", "lpcSynthesize", "codecHighPassFilter")


def _functions():
    out = {}
    for k in KEYS:
        out.setdefault(R.port_name(k), []).append(k)
    return out


@pytest.fixture(scope="module")
def codec():
    lib = R.load_lib()
    if "snd.codecDecodeFrame" not in lib.functions():
        pytest.skip("the DLL has no audio_codec module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def codec32():
    from port.test import lib32 as L32
    try:
        lib = R.load_w32()
    except (L32.RunnerUnavailable, FileNotFoundError, OSError) as e:
        pytest.skip(f"Win32 backend unavailable: {e}")
    if "snd.codecDecodeFrame" not in lib.functions():
        pytest.skip("the Win32 DLL has no audio_codec module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def golden():
    g = R.load_golden(STEP)
    if not g:
        pytest.skip("no golden digests (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.3 --update)")
    return g


# ================================================================================================ static
@pytest.mark.static
def test_every_step_row_has_a_key():
    rows = R.step_rows(STEP)
    assert len(rows) == 42
    assert {r["name"] for r in rows} == {R.row_name(k) for k in KEYS}


@pytest.mark.static
def test_goldens_cover_every_key_at_the_default_word():
    g = R.load_golden(STEP)
    if not g:
        pytest.skip("no golden file")
    for k in KEYS:
        assert k in g, k
        assert g[k]["vectors"] == len(R.spec_of(k)["vectors"]) == len(g[k]["digests"]) == len(g[k]["x87_exit"]), k
        assert g[k]["tier"] in ("EXACT", "EQUAL_PI"), k
        assert g[k].get("fpcw", [R.DEFAULT_CW]) == [R.DEFAULT_CW], k   # U-1421: no vector at another word


@pytest.mark.static
def test_module_source_rules():
    src = (ROOT / "port" / "src" / "sim" / "audio_codec.cpp").read_text()
    assert "TODO(convert)" not in src
    assert src.count("neg_u32(") + src.count("neg_mem(") >= 42
    # one stack-locals helper: every frame goes through CodecFrame, over the core's SndFrame
    code = [ln for ln in src.splitlines() if not ln.lstrip().startswith("//")]
    assert sum("SndFrame" in ln for ln in code) == 1 and not any("SndLocals" in ln for ln in code)
    assert "class CodecFrame" in src and "cc_unmodelled()" in src
    for fn in SPILL_CONTROLS:
        assert f"NEG({fn}) == 2" in src, fn


# ================================================================================================ the module in the library
def test_module_registers_every_row(codec):
    have = codec.functions()
    names = {"snd." + r["name"] for r in R.step_rows(STEP)}
    assert names <= set(have), sorted(names - set(have))
    assert all(have[n]["module"] == "audio_codec" for n in names)


def _replay(lib, key, golden):
    if R.load_bundle(key) is None:
        pytest.skip(f"no bundle for {key} (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.3 --update)")
    return R.replay_key(lib, key, golden)


@pytest.mark.parametrize("key", KEYS)
def test_key_equals_golden_x64(codec, golden, key):
    r = _replay(codec, key, golden)
    assert R.replay_ok(r), r


@pytest.mark.x87
def test_every_key_equals_golden_win32(codec32, golden):
    bad = []
    for key in KEYS:
        r = _replay(codec32, key, golden)
        if not R.replay_ok(r):
            bad.append((key, r.passed, r.vectors, r.first))
    assert not bad, bad


def _bad(lib, golden, fn, mode, keys):
    lib.negctl(fn, mode)
    try:
        return sum(r.vectors - r.passed for r in (_replay(lib, k, golden) for k in keys))
    finally:
        lib.negctl(fn, 0)


@pytest.mark.parametrize("fn", sorted(_functions()))
def test_negctl_red_against_golden(codec, golden, fn):
    assert _bad(codec, golden, fn, 1, _functions()[fn]) > 0, f"{fn}: mode 1 reads GREEN on every vector"


@pytest.mark.parametrize("fn", SPILL_CONTROLS)
def test_spill_control_red_against_golden(codec, golden, fn):
    """Mode 2: the body keeps one intermediate the original spills through `fstp dword` as a double. The vectors must see the
    missing float rounding (log/roadmap/S4/3_negctl_m2.tsv: codecHighPassFilter 11/14, lpcSynthesize 3/14, codecRemoveMean 1/14)."""
    assert _bad(codec, golden, "snd." + fn, 2, _functions()["snd." + fn]) > 0, f"{fn}: mode 2 reads GREEN on every vector"


@pytest.mark.x87
@pytest.mark.parametrize("fn", SPILL_CONTROLS)
def test_spill_control_red_on_win32(codec32, golden, fn):
    assert _bad(codec32, golden, "snd." + fn, 2, _functions()["snd." + fn]) > 0


def test_intpow_zero_base_negative_exponent_is_a_divide_error(codec):
    """intPow's `idiv` by a zero base (rva 0x3e313, reached for a base of 0 and an exponent <= 0 other than 0) is the one arm no
    vector executes (log/roadmap/S4/3_coverage.tsv): the original raises #DE, the port reports SG_E_DIVIDE."""
    st = codec.state()
    try:
        st.map(0x30000000, bytearray(struct.pack("<ii", 0, -3)))
        with pytest.raises(L.SgError) as e:
            st.call("snd.intPow", [0x30000000, 0x30000004])
        assert e.value.code == L.SG_E_DIVIDE
        st.unmap_all()
        st.map(0x30000000, bytearray(struct.pack("<ii", -1, -3)))
        assert st.call("snd.intPow", [0x30000000, 0x30000004]).eax == 0xFFFFFFFF   # (-1)^3 after `neg` at 0x3e32a
    finally:
        st.close()


# ================================================================================================ the live oracle (opt-in)
@pytest.fixture(scope="module")
def vm():
    if not ORACLE:
        pytest.skip("SG_EMU_ORACLE=1 runs the emulator")
    with R.E.FixVM() as v:
        yield v


@pytest.mark.parametrize("key", KEYS)
def test_oracle_key(vm, codec, key):
    v, _, _ = R.oracle_key(vm, codec, key)
    assert v.verdict in ("GREEN", "GREEN_PENDING") and v.errors == 0, v
    g = R.load_golden(STEP).get(key)
    if g and not v.forgiven:
        assert v.golden == g["digests"], "the emulated original no longer gives the pinned digests"


@pytest.mark.parametrize("fn", sorted(_functions()))
def test_oracle_negctl_red(vm, codec, fn):
    codec.negctl(fn, 1)
    try:
        bad = 0
        for k in _functions()[fn]:
            v, _, _ = R.oracle_key(vm, codec, k, ("A",), record=False)
            bad += v.vectors - v.match
    finally:
        codec.negctl(fn, 0)
    assert bad > 0
