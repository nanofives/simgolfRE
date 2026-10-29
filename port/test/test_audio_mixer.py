"""Step S4.4: sound.dll's mixers and channel renderers (module audio_mixer) judged against the emulated original
(decision G1; log/roadmap/S4/4_notes.md). Since S4.2b the harness is port/test/sound_replay.py's for every step (the x87 unit
per vector, the stack, the derived MMX keys on a zeroed emulator stack and the two chained keys are its tables), and the x87
unit is the audio core's (portsim/sim/audio.h): this file holds the S4.4 tests and the x87 operation probes.

    ~/bin/py -3.12 -m pytest port/test/test_audio_mixer.py [--backend win32]      # bundles + goldens + x87 probes
    SG_EMU_ORACLE=1 ~/bin/py -3.12 -m pytest port/test/test_audio_mixer.py        # + the live oracle per key
    ~/bin/py -3.12 -m port.test.sound_replay oracle --step S4.4 [--update] [--coverage <tsv>]
    ~/bin/py -3.12 -m port.test.sound_replay negctl --step S4.4 --mode 1|2
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
from port.test import sound_replay as S  # noqa: E402

E = S.E
L = S.L
STEP = "S4.4"
ORACLE = os.environ.get("SG_EMU_ORACLE") == "1"
KEYS = S.keys_for(STEP)
RESAMPLERS = ("Voice_mixAddMono16", "Voice_mixSetMono16", "Voice_mixSet8bit", "Voice_mixAdd8bit", "Voice_mixSetStereo16",
              "Voice_mixAddStereo16", "streamRenderBlock", "streamRenderTail")
pytestmark = pytest.mark.port


# ---- the x87 probe (operation by operation against the emulator) --------------------------------------------------------
# Each record is one x87 instruction on random / edge operands under one of eight control words (precision 24 / 53 / 64 by
# rounding nearest / down / up / chop): the core's operation (audio_core.x80batch) against the same instruction in Unicorn.
X80_OPS = {0: "add", 1: "sub", 2: "mul", 3: "div", 4: "fst64", 5: "fist16", 6: "fist32", 7: "fist64", 8: "fcom",
           9: "fld64", 11: "fild64"}
X80_WORDS = (0x027F, 0x0F7F, 0x037F, 0x007F, 0x0B7F, 0x077F, 0x067F, 0x0E7F)


def _dbl_x80(bits):
    s, ex, f = bits >> 63, (bits >> 52) & 0x7FF, bits & ((1 << 52) - 1)
    if ex == 0x7FF:
        return ((1 << 63) | (f << 11), (s << 15) | 0x7FFF)
    if ex == 0:
        if f == 0:
            return (0, s << 15)
        c = 64 - f.bit_length()
        return (f << c, (s << 15) | (16383 + 63 - 1074 - c))
    return ((1 << 63) | (f << 11), (s << 15) | (ex - 1023 + 16383))


def _rand_double(rng):
    k = rng.random()
    if k < 0.6:
        return struct.unpack("<Q", struct.pack("<d", rng.uniform(-70000, 70000) * rng.choice((1, 1e-3, 1e3, 1 / 3))))[0]
    if k < 0.8:
        return rng.getrandbits(64)
    return rng.choice([0, 1 << 63, 0x7FF0000000000000, 0xFFF0000000000000, 0x7FF8000000000000, 0x7FF0000000000001,
                       0x0000000000000001, 0x000FFFFFFFFFFFFF, 0x0010000000000000, 0x7FEFFFFFFFFFFFFF,
                       0x3FF0000000000000, 0x4000000000000000, 0x3FD7384EF2A605CE])


def _rand_x80(rng):
    k = rng.random()
    if k < 0.35:
        return _dbl_x80(_rand_double(rng))
    if k < 0.75:
        return (rng.getrandbits(64) | (1 << 63), (rng.getrandbits(1) << 15) | rng.randint(16383 - 80, 16383 + 80))
    if k < 0.85:
        m = (1 << 63) | rng.choice((0, 1, 2, 3, (1 << 63) - 1, rng.getrandbits(63)))
        return (m, (rng.getrandbits(1) << 15) | (16383 + rng.randint(-3, 3)))
    return rng.choice([(0, 0), (0, 0x8000), (1 << 63, 0x7FFF), (1 << 63, 0xFFFF), (0xC000000000000000, 0xFFFF),
                       (0xC000000000000001, 0x7FFF), (0xA000000000000000, 0x7FFF), (0xE000000000000000, 0xFFFF),
                       ((1 << 64) - 1, 0x403E), (1 << 63, 0x401E), (1 << 63, 0x403E), ((1 << 63) | 1, 0x3FFE)])


def x80_records(n, seed):
    import random
    rng = random.Random(seed)
    recs = []
    for _ in range(n):
        op = rng.choice(list(X80_OPS))
        cw = rng.choice(X80_WORDS)
        if op == 9:
            x = (_rand_double(rng), 0)
        elif op == 11:
            x = (rng.choice([rng.getrandbits(64), rng.getrandbits(20), 0, 1 << 63, (1 << 64) - 1]), 0)
        elif op in (5, 6, 7) and rng.random() < 0.5:
            x = _dbl_x80(struct.unpack("<Q", struct.pack("<d", rng.uniform(-3e9, 3e9) * rng.choice((1, 1e-4, 1e6))))[0])
        else:
            x = _rand_x80(rng)
        y = _rand_x80(rng)
        if op in (0, 1) and rng.random() < 0.3:      # nearby magnitudes: cancellation
            y = ((x[0] ^ rng.getrandbits(rng.randint(1, 70))) & ((1 << 64) - 1) | (1 << 63), x[1] ^ rng.choice((0, 0x8000)))
        recs.append((op, cw, x, y))
    return recs


def x80_port(lib, recs, soft=False):
    st = lib.state()
    try:
        st.files_put("$audio/x80in", b"".join(struct.pack("<IIQHQHI", op, cw, x[0], x[1], y[0], y[1], 0)
                                              for op, cw, x, y in recs))
        n = st.call("audio_core.x80batch", [1 if soft else 0]).eax
        out = st.files_read("$audio/x80out")
    finally:
        st.close()
    assert n == len(recs)
    return [struct.unpack_from("<QH", out, 16 * i) for i in range(n)]


def x80_emu(recs):
    """The same instructions in Unicorn: fninit; fldcw; the operands as tbytes (qwords for fld / fild); the instruction."""
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
    from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_FPSW
    code_va, data = 0x10000, 0x20000
    a_va, b_va, r_va, cw_va = data, data + 0x10, data + 0x20, data + 0x30
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    for base in (code_va, data, 0x80000):
        uc.mem_map(base, 0x1000)
    pre = b"\xdb\xe3\xd9\x2d" + struct.pack("<I", cw_va)                     # fninit; fldcw [cw]
    fld = lambda va: b"\xdb\x2d" + struct.pack("<I", va)                     # noqa: E731  fld tbyte
    fstp_r = b"\xdb\x3d" + struct.pack("<I", r_va)                           # fstp tbyte [r]
    code = {0: pre + fld(b_va) + fld(a_va) + b"\xd8\xc1" + fstp_r, 1: pre + fld(b_va) + fld(a_va) + b"\xd8\xe1" + fstp_r,
            2: pre + fld(b_va) + fld(a_va) + b"\xd8\xc9" + fstp_r, 3: pre + fld(b_va) + fld(a_va) + b"\xd8\xf1" + fstp_r,
            4: pre + fld(a_va) + b"\xdd\x1d" + struct.pack("<I", r_va), 5: pre + fld(a_va) + b"\xdf\x1d" + struct.pack("<I", r_va),
            6: pre + fld(a_va) + b"\xdb\x1d" + struct.pack("<I", r_va), 7: pre + fld(a_va) + b"\xdf\x3d" + struct.pack("<I", r_va),
            8: pre + fld(b_va) + fld(a_va) + b"\xde\xd9", 9: pre + b"\xdd\x05" + struct.pack("<I", a_va) + fstp_r,
            11: pre + b"\xdf\x2d" + struct.pack("<I", a_va) + fstp_r,
            # fyl2x: st(1) = b, st(0) = a; fyl2x; fstp tbyte
            12: pre + fld(b_va) + fld(a_va) + b"\xd9\xf1" + fstp_r}
    out = []
    for op, cw, x, y in recs:
        uc.mem_write(data, b"\0" * 0x40)
        uc.mem_write(a_va, struct.pack("<Q", x[0]) if op in (9, 11) else struct.pack("<QH", *x))
        uc.mem_write(b_va, struct.pack("<QH", *y))
        uc.mem_write(cw_va, struct.pack("<H", cw))
        c = code[op]
        uc.mem_write(code_va, c)
        uc.reg_write(UC_X86_REG_ESP, 0x88000)
        uc.emu_start(code_va, code_va + len(c))
        if op == 8:
            out.append((0, uc.reg_read(UC_X86_REG_FPSW) & 0x4700))
            continue
        r = bytes(uc.mem_read(r_va, 10))
        if op in (0, 1, 2, 3, 9, 11, 12):
            out.append(struct.unpack("<QH", r))
        else:
            n = {4: 8, 5: 2, 6: 4, 7: 8}[op]
            out.append((int.from_bytes(r[:n], "little", signed=True) & (2 ** 64 - 1), 0))
    return out


def is_snan_move(op, x):
    """A signaling NaN through fld qword (op 9) or fst qword of an SNaN register (op 4): the x87 sets the quiet bit,
    Unicorn 2.1.4 does not (U-1430); the port follows the hardware."""
    if op == 9:
        b = x[0]
        return (b >> 52) & 0x7FF == 0x7FF and b & ((1 << 52) - 1) and not (b >> 51) & 1
    if op == 4:
        return x[1] & 0x7FFF == 0x7FFF and x[0] >> 63 and (x[0] << 1) & (2 ** 64 - 1) and not (x[0] >> 62) & 1
    return False


def nan_tie_records():
    """Two NaNs with equal significands (quiet / signaling, both signs, both orders) through add / sub / mul / div."""
    recs = []
    for sig in (0xC000000000000000, 0xC123456789ABCDEF, 0x8000000000000001, 0xBFFFFFFFFFFFFFFF):
        for sa in (0, 0x8000):
            for sb in (0, 0x8000):
                for op in (0, 1, 2, 3):
                    recs.append((op, 0x027F, (sig, 0x7FFF | sa), (sig, 0x7FFF | sb)))
    return recs


def fyl2x_records(n, seed):
    """fyl2x operands in the range DsBuffer::setVolume feeds it (st(0) = e * (1/127), e a positive __ftol result; st(1) =
    ln 2) and random positive values, at the eight words."""
    import random
    rng = random.Random(seed)
    k = struct.unpack("<Q", struct.pack("<d", 1 / 127))[0]
    ln2 = (0xB17217F7D1CF79AC, 0x3FFE)
    recs = []
    for _ in range(n):
        cw = rng.choice(X80_WORDS)
        if rng.random() < 0.6:
            e = rng.randint(1, 2_100_000)
            x = _dbl_x80(struct.unpack("<Q", struct.pack("<d", e * (1 / 127)))[0])
            y = ln2
        else:
            x = (rng.getrandbits(64) | (1 << 63), rng.randint(16383 - 40, 16383 + 40))
            y = _rand_x80(rng)
            if y[1] & 0x7FFF in (0, 0x7FFF):
                y = ln2
        recs.append((12, cw, x, y))
    del k
    return recs


@pytest.mark.port
def test_x87_operations_equal_the_emulator(lib):
    """Every operation of the unit equals Unicorn's x87 on 6,000 records (eight control words), except the signaling-NaN
    moves, where it equals the hardware (quiet bit set) and the emulator does not."""
    recs = x80_records(6000, 44)
    port, emu = x80_port(lib, recs), x80_emu(recs)
    bad = [(i, X80_OPS[r[0]], hex(r[1])) for i, (r, p, e) in enumerate(zip(recs, port, emu)) if p != e and not is_snan_move(r[0], r[2])]
    assert not bad, bad[:10]
    snan = [i for i, r in enumerate(recs) if is_snan_move(r[0], r[2])]
    assert snan and all(port[i] != emu[i] for i in snan)


@pytest.mark.port
def test_x87_nan_ties_equal_the_emulator(lib):
    """Equal-significand NaN pairs (U-1434): the positive operand wins, as Unicorn's x87 and the Win32 instructions do."""
    recs = nan_tie_records()
    port, emu = x80_port(lib, recs), x80_emu(recs)
    assert port == emu, [(i, recs[i]) for i, (p, e) in enumerate(zip(port, emu)) if p != e][:5]


@pytest.mark.port
def test_x87_fyl2x_equals_the_emulator(lib):
    """S4.2b: the unit's fyl2x (the reference emulator's model, U-1471) equals Unicorn's fyl2x on 3,000 records, setVolume's
    operand range included, at the eight control words."""
    recs = fyl2x_records(3000, 46)
    port, emu = x80_port(lib, recs), x80_emu(recs)
    bad = [(i, recs[i], p, e) for i, (p, e) in enumerate(zip(port, emu)) if p != e]
    assert not bad, bad[:5]


@pytest.mark.port
def test_x87_win32_instructions_equal_the_software_path():
    """Win32: the unit's operations as x87 instructions (the default there) equal the software path record for record,
    signaling NaNs included (what x64 runs)."""
    try:
        lib32 = S.load_w32()
    except Exception as e:  # noqa: BLE001
        pytest.skip(f"no Win32 runner: {e}")
    recs = x80_records(6000, 45) + nan_tie_records() + fyl2x_records(500, 47)
    hw, soft = x80_port(lib32, recs, soft=False), x80_port(lib32, recs, soft=True)
    bad = [(i, X80_OPS.get(r[0], "fyl2x")) for i, (r, a, b) in enumerate(zip(recs, hw, soft)) if a != b]
    assert not bad, bad[:10]


# ---- the module's keys ----------------------------------------------------------------------------------------------------
def _lib(request):
    if request.config.getoption("--backend", default="x64") == "win32" or os.environ.get("SG_BACKEND") == "win32":
        return S.load_w32()
    return S.load_lib()


@pytest.fixture(scope="module")
def lib(request):
    try:
        lib = _lib(request)
    except Exception as e:  # noqa: BLE001
        pytest.skip(f"no S4 build: {e}")
    if "snd.renderChannelSingle" not in lib.functions():
        pytest.skip("the DLL has no audio_mixer module")
    yield lib
    lib.negctl_reset()


@pytest.fixture(scope="module")
def golden():
    g = S.load_golden(STEP)
    if not g:
        pytest.skip("no golden_audio_mixer.json")
    return g


@pytest.mark.static
def test_every_row_has_a_key_and_a_golden():
    names = {r["name"] for r in S.step_rows(STEP)}
    assert len(names) == 38 and {S.SND + n for n in names} == {S.port_name(k) for k in KEYS}
    g = S.load_golden(STEP)
    if not g:
        pytest.skip("no golden file")
    for k in KEYS:
        assert k in g and len(g[k]["digests"]) == g[k]["vectors"] == len(g[k]["x87_exit"]), k


@pytest.mark.parametrize("key", KEYS)
def test_replay_key(lib, golden, key):
    """Each key from its bundle against the emulator's golden digests and x87 exit images (x64 or Win32)."""
    if S.load_bundle(key) is None:
        pytest.skip("no bundle (local corpus): run the oracle with --update")
    r = S.replay_key(lib, key, golden)
    assert S.replay_ok(r), r.first


def _functions():
    out = {}
    for k in KEYS:
        out.setdefault(S.port_name(k), []).append(k)
    return out


def _red(lib, golden, fn, mode, keys):
    for k in keys:
        if S.load_bundle(k) is None:
            pytest.skip("bundles missing")
    lib.negctl(fn, mode)
    try:
        return sum(r.vectors - r.passed for r in (S.replay_key(lib, k, golden) for k in keys))
    finally:
        lib.negctl(fn, 0)


@pytest.mark.parametrize("fn", sorted(_functions()))
def test_negctl_red_against_golden(lib, golden, fn):
    """Mode 1 of every function: at least one vector no longer hashes to the emulator's digests."""
    assert _red(lib, golden, fn, 1, _functions()[fn]) > 0


def test_ftol_negctl_red_against_golden(lib, golden):
    """sound.dll's __ftol (the core's audio_core.crt.ftol), mode 1 (the caller's rounding kept): RED through the renderer keys
    that call it."""
    keys = [k for k in KEYS if k.startswith(("renderChannelStreamed_fade", "renderChannelVoice"))]
    assert keys and _red(lib, golden, "audio_core.crt.ftol", 1, keys) > 0


@pytest.mark.parametrize("fn", RESAMPLERS)
def test_long_double_control_red_against_golden(lib, golden, fn):
    """Mode 2, the c3fh long-double control (every intermediate a 53-bit double rounded to nearest): RED."""
    assert _red(lib, golden, S.SND + fn, 2, _functions()[S.SND + fn]) > 0


@pytest.mark.skipif(not ORACLE, reason="SG_EMU_ORACLE=1 runs the emulated original")
@pytest.mark.parametrize("key", KEYS)
def test_oracle_key(key):
    assert S.oracle_main([key], "ABC") == 0
