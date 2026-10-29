"""Step S4.8 part 1, module audio_ds (log/roadmap/S4/8_notes.md): sound.dll's DirectSound boundary (DsBuffer, Ds3DBuffer,
Ds3DListener, DirectSoundDevice and the module's DsBuffer list), judged against the emulated original (decision G1).

The replay front end is port/test/sound_replay.py (`--step S4.8 --family ds|dev`; S4.8's log/roadmap/S4/8_replay.py until
S4.2b folded its goldens per module, custom keys, entry esp and import-log check into it). Default runs replay the
bundles (port/corpus/audio, local) against port/test/golden_audio_ds.json, x64 and Win32; SG_EMU_ORACLE=1 runs the live
oracle per key and the exhaustive setVolume check.

    ~/bin/py -3.12 -m pytest port/test/test_audio_ds.py
    SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_ds.py -k oracle
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
FAMILY = "ds"
ORACLE = os.environ.get("SG_EMU_ORACLE") == "1"
KEYS = R.keys_for("S4.8", FAMILY)
SOUND_DLL = ROOT / "original" / "sound.dll"


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
    if "snd.DsBuffer::setVolume" not in lib.functions():
        pytest.skip("the DLL has no audio_ds module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def sndlib32():
    from port.test import lib32 as L32
    try:
        lib = R.load_w32()
    except L32.RunnerUnavailable as e:
        pytest.skip(f"Win32 backend unavailable: {e}")
    if "snd.DsBuffer::setVolume" not in lib.functions():
        pytest.skip("the Win32 DLL has no audio_ds module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def golden():
    g = R.load_golden("S4.8", FAMILY)
    if not g:
        pytest.skip("no golden digests (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.8 --family ds --update)")
    return g


def _replay(lib, key, golden):
    if R.load_bundle(key) is None:
        pytest.skip(f"no bundle for {key} (SG_EMU_ORACLE=1 ~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.8 --family ds --update)")
    return R.replay_key(lib, key, golden)


# ================================================================================================ static
@pytest.mark.static
def test_every_row_has_a_key():
    rows = [r for r in R.step_rows("S4.8") if r["family"].rstrip("~") == FAMILY]
    assert len(rows) == 54
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
        assert g[k].get("fallbacks", 0) == 0, k


# ================================================================================================ default: bundles + goldens
@pytest.mark.parametrize("key", KEYS)
def test_key_equals_golden_x64(sndlib, golden, key):
    r = _replay(sndlib, key, golden)
    assert r.vectors and r.passed == r.vectors and r.fallbacks == 0, r


@pytest.mark.x87
def test_every_key_equals_golden_win32(sndlib32, golden):
    bad = []
    for key in KEYS:
        if R.load_bundle(key) is None:
            pytest.skip("bundles missing")
        r = R.replay_key(sndlib32, key, golden)
        if not (r.vectors and r.passed == r.vectors and r.fallbacks == 0):
            bad.append((key, r.passed, r.vectors, r.first))
    assert not bad, bad


# mode 1 of every function, plus setVolume's mode 2 (every step at 0x027f: the control word matters on the 0x0f7f key)
NEGCTL = [(fn, 1) for fn in sorted(_functions())] + [("snd.DsBuffer::setVolume", 2)]


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


# ================================================================================================ the stack and the control word
def test_locals_fall_back_to_the_frame_block(sndlib):
    """With no entry esp given, a function's by-address locals go to the module's frame block: DsBuffer::isBusy (`push ecx`
    local, GetStatus's out-parameter) on a sink interface object writes its status there and reads it back."""
    if not SOUND_DLL.exists():
        pytest.skip("original/sound.dll absent")
    st = sndlib.state()
    try:
        st.files_put("sound.dll", SOUND_DLL.read_bytes())
        st.call("audio_core.attach", [0x04C00000, 1])
        blk = st.map(0x30000000, bytearray(0x1000))
        st.call("audio_core.useSink", [0x55])
        obj = st.call("audio_core.sinkObject", [7, 16]).eax
        struct.pack_into("<I", blk, 0x60, obj)
        assert st.call("snd.DsBuffer::isBusy", [0x30000000]).eax == 0x30000000 & 1
        st.call("audio_core.sinkLog", [])
        line = st.files_read("$audio/sink").decode().splitlines()[0].split()
        assert line[:2] == ["com", "7.9"] and int(line[2]) == obj
        assert 0xFEB00000 <= int(line[3]) < 0xFEB10000          # the core's frame block (portsim/sim/audio.h)
    finally:
        st.close()


def test_locals_at_the_entry_esp_and_at_the_callees_entry(sndlib):
    """S4.2b's one stack: with an entry esp E given ("audio_core.esp"), isBusy's `push ecx` local is at E - 4 (GetStatus's
    out-parameter in the sink command); called through the dispatcher (its thunk 0x1b9f, thiscall, no stack argument) it
    enters at E - 4, the esp a `call` leaves, so the local is at E - 8. A frame below the given limit is refused."""
    if not SOUND_DLL.exists():
        pytest.skip("original/sound.dll absent")
    st = sndlib.state()
    try:
        st.files_put("sound.dll", SOUND_DLL.read_bytes())
        st.call("audio_core.attach", [0x04C00000, 1])
        blk = st.map(0x30000000, bytearray(0x1000))
        st.map(0x31000000, bytearray(0x2000))                    # a stack
        stack_top = 0x31001F00
        st.call("audio_core.useSink", [0x55])
        obj = st.call("audio_core.sinkObject", [7, 16]).eax
        struct.pack_into("<I", blk, 0x60, obj)
        st.call("audio_core.esp", [stack_top, 0x31000000])
        st.call("snd.DsBuffer::isBusy", [0x30000000])
        st.files_put("$audio/args", struct.pack("<III", 2, 1, 0x30000000))
        st.call("audio_core.dispatch", [0x04C00000 + 0x1b9f])
        st.call("audio_core.sinkLog", [])
        lines = [ln.split() for ln in st.files_read("$audio/sink").decode().splitlines()]
        assert [int(ln[3]) for ln in lines] == [stack_top - 4, stack_top - 8], lines
        st.call("audio_core.esp", [stack_top, stack_top - 2])     # the limit above the frame
        with pytest.raises(L.SgError) as e:
            st.call("snd.DsBuffer::isBusy", [0x30000000])
        assert e.value.code == L.SG_E_NO_RESOURCE
    finally:
        st.close()


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
    assert v.match == v.vectors and v.match_bc == 2 * v.vectors and v.fallbacks == 0 and v.errors == 0, v
    g = R.load_golden("S4.8", FAMILY).get(key)
    if g:
        assert v.golden == g["digests"], "the emulated original no longer gives the pinned digests"


def _emu_setvolume(words, masters, vols):
    """The ORIGINAL DsBuffer::setVolume (rva 0xf1c0) in Unicorn over the user's sound.dll, no fixture VM: an object with no
    interface (+0x60 = 0: eax is the volume) and a master (+0x5c) whose +0x188 is the level; None = no master."""
    base = 0x04C00000
    img = E.load_image(base)
    m = E.Machine(None)
    m._map(base, (len(img.data) + 0xFFF) & ~0xFFF)
    m.uc.mem_write(base, img.data)
    obj, mst = 0x30000000, 0x30001000
    m._map(obj, 0x2000)
    out = {}
    for cw in words:
        for mv in masters:
            m.write(obj + 0x5c, struct.pack("<I", 0 if mv is None else mst))
            m.write(obj + 0x60, struct.pack("<I", 0))
            m.write(mst + 0x188, struct.pack("<I", 0 if mv is None else mv))
            for v in vols:
                m.uc.reg_write(E.UC_X86_REG_FPCW, cw)
                r = m.native_call(base + 0xf1c0, "thiscall", [{"t": "pointer", "v": obj}, {"t": "uint32", "v": v}],
                                  "uint32")
                out[(cw, mv, v)] = (r["eax"] & 0xFFFFFFFF, struct.unpack("<I", m.read(obj + 0xc4, 4))[0])
    return out


def _port_setvolume(lib, words, masters, vols):
    st = lib.state()
    out = {}
    try:
        st.files_put("sound.dll", SOUND_DLL.read_bytes())
        st.call("audio_core.attach", [0x04C00000, 1])
        blk = st.map(0x30000000, bytearray(0x2000))
        for cw in words:
            # the caller's control word is the x87 unit's (S4.2b; S4.8's audio_dev.fpcw until then)
            st.files_put("$audio/x87", b"SX87" + struct.pack("<HHHH", cw, 0, 0xFFFF, 0) + bytes(80))
            st.call("audio_core.x87load", [])
            for mv in masters:
                struct.pack_into("<II", blk, 0x5c, 0 if mv is None else 0x30001000, 0)
                struct.pack_into("<I", blk, 0x1188, 0 if mv is None else mv)
                for v in vols:
                    e = st.call("snd.DsBuffer::setVolume", [0x30000000, v]).eax
                    out[(cw, mv, v)] = (e, struct.unpack_from("<I", blk, 0xc4)[0])
    finally:
        st.close()
    return out


SV_WORDS = (0x027F, 0x0F7F, 0x037F, 0x0E7F)
SV_MASTERS = [None] + list(range(0, 128)) + [0x80, 0xFF, 0x1000, 0x7FFFFFFF, 0xFFFFFFFF]
SV_VOLS = list(range(0, 129)) + [0xFFFFFFFF]


def test_oracle_setvolume_exhaustive(sndlib):
    """Every master level and volume (and the clamps) at four control words: port == the emulated original. Closes the
    blind spot of setVolume's mode 3 on the 30 vectors (the first product's precision), which this grid reads RED."""
    if not ORACLE:
        pytest.skip("SG_EMU_ORACLE=1 runs the emulator")
    if not SOUND_DLL.exists():
        pytest.skip("original/sound.dll absent")
    want = _emu_setvolume(SV_WORDS, SV_MASTERS, SV_VOLS)
    got = _port_setvolume(sndlib, SV_WORDS, SV_MASTERS, SV_VOLS)
    diff = [k for k in want if want[k] != got[k]]
    assert not diff, (len(diff), [(k, want[k], got[k]) for k in diff[:5]])
    sndlib.negctl("snd.DsBuffer::setVolume", 3)
    try:
        neg = _port_setvolume(sndlib, SV_WORDS, SV_MASTERS, SV_VOLS)
    finally:
        sndlib.negctl("snd.DsBuffer::setVolume", 0)
    assert any(want[k] != neg[k] for k in want), "mode 3 reads GREEN on the whole grid"
