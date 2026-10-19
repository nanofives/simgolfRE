"""The Win32 backend (phase 3.R): real x87 through port/test/runner32, next to the x64 in-process library.

Everything here needs the Win32 build (`port/test/build.py --arch Win32`, done on demand); without a toolchain the `lib32`
fixture skips with the reason. Marker `x87` selects the tests that exist because of the Win32 path.
"""
from __future__ import annotations

import fractions
import pathlib
import random
import struct
import sys
import time

import pytest

from port.test import build as B
from port.test import lib as L
from port.test import lib32 as L32
from port.test import segments as S
from port.test import vectors as V

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = [pytest.mark.port, pytest.mark.x87]
need_corpus = pytest.mark.skipif(not VL.VEC_DIR.exists(), reason="vector corpus not present (port/corpus/vectors, local)")
need_segments = pytest.mark.skipif(not S.SEG_DIR.exists(), reason="segment corpus not present (port/corpus/segments, local)")

CW_NEAREST_53, CW_NEAREST_64, CW_CHOP_64 = 0x027F, 0x037F, 0x0F7F
CW_DOWN_64, CW_UP_64, CW_NEAREST_24 = 0x077F, 0x0B7F, 0x007F


# ------------------------------------------------------------------------------------------- the x87 reference
def _round(x: fractions.Fraction, bits: int, rc: int) -> fractions.Fraction:
    """x rounded to a `bits`-bit significand by x87 rounding control rc (0 nearest even, 1 down, 2 up, 3 toward zero)."""
    if x == 0:
        return x
    neg = x < 0
    m = -x if neg else x
    e = m.numerator.bit_length() - m.denominator.bit_length() - bits
    while m >= fractions.Fraction(2) ** (e + bits):
        e += 1
    while m < fractions.Fraction(2) ** (e + bits - 1):
        e -= 1
    q = m / fractions.Fraction(2) ** e                      # in [2^(bits-1), 2^bits)
    fl = q.numerator // q.denominator
    rem = q - fl
    away = (rc == 2 and not neg) or (rc == 1 and neg)
    if rc == 0:
        up = rem > fractions.Fraction(1, 2) or (rem == fractions.Fraction(1, 2) and fl & 1)
    else:
        up = bool(rem) and away
    r = (fl + (1 if up else 0)) * fractions.Fraction(2) ** e
    return -r if neg else r


def mul_div_ref(a: float, b: float, c: float, cw: int) -> float:
    """(a * b) / c as x87 computes it under control word cw: both operations round to the precision control's
    significand by the rounding control, the store to a double rounds to 53 bits by the same rounding control."""
    bits = {0: 24, 2: 53, 3: 64}[(cw >> 8) & 3]
    rc = (cw >> 10) & 3
    fa, fb, fc = (fractions.Fraction(v) for v in (a, b, c))
    p = _round(fa * fb, bits, rc)
    q = _round(p / fc, bits, rc)
    return float(_round(q, 53, rc))


def _triples(n: int = 24, seed: int = 319) -> list[tuple[float, float, float]]:
    rng = random.Random(seed)
    return [(rng.uniform(1, 2) * 2.0 ** rng.randrange(-8, 8), rng.uniform(1, 2) * 2.0 ** rng.randrange(-8, 8),
             rng.uniform(1, 2) * 2.0 ** rng.randrange(-8, 8)) for _ in range(n)]


def _slots(a: float, b: float, c: float) -> list[int]:
    return list(struct.unpack("<6I", struct.pack("<3d", a, b, c)))


def _doc(cw: int, triples, expected) -> dict:
    vec = [{"i": i, "status": "ok", "args": [{"float": a}, {"float": b}, {"float": c}], "ret": e, "out": [], "in": []}
           for i, ((a, b, c), e) in enumerate(zip(triples, expected))]
    key = {"key": f"probe_{cw:04x}", "abi": "default", "ret": "double", "args": ["double"] * 3, "blocks": [], "seed": [],
           "relocs": [], "x87": True, "fpcw": cw, "vectors": vec}
    return {"function": "x87Probe::mulDiv", "keys": [key]}


# ------------------------------------------------------------------------------------------- static (no runner needed)
@pytest.mark.static
def test_reference_is_ieee_at_the_default_word():
    """The Fraction reference equals Python's double arithmetic where x87 53-bit nearest equals SSE2 (two rounded ops)."""
    for a, b, c in _triples(200):
        assert mul_div_ref(a, b, c, CW_NEAREST_53) == a * b / c


@pytest.mark.static
def test_missing_toolchain_is_a_reason_not_a_failure(monkeypatch, tmp_path):
    monkeypatch.setattr(B, "runner32_path", lambda config="Release": tmp_path / "none.exe")
    monkeypatch.setattr(B, "toolchain_ok", lambda: False)
    why = L32.available()
    assert "no Win32 toolchain" in why
    with pytest.raises(L32.RunnerUnavailable):
        L32.Lib32(rebuild=False)


@pytest.mark.static
def test_backend_selection(monkeypatch):
    monkeypatch.delenv("SG_BACKEND", raising=False)
    assert L.backend() == "x64"
    monkeypatch.setenv("SG_BACKEND", "WIN32")
    assert L.backend() == "win32"
    monkeypatch.setenv("SG_BACKEND", "arm")
    with pytest.raises(ValueError):
        L.backend()


# ------------------------------------------------------------------------------------------- the runner itself
def test_runner_basics(lib32, lib64):
    assert "x86" in lib32.build_info() and "x64" in lib64.build_info()
    assert lib32.state_size == lib64.state_size and lib32.pointer_size == 4
    assert lib32.functions() == lib64.functions()
    assert lib32.objects("sim") == lib64.objects("sim") and len(lib32.objects()) == len(lib64.objects()) == 989
    assert lib32.hash_bytes(b"abc") == lib64.hash_bytes(b"abc")
    assert lib32.defects() == lib64.defects()


def test_runner_state_roundtrip(lib32):
    """GameState bytes edited in Python reach the DLL and come back; a writable block travels with the call."""
    with lib32.state() as st:
        st.set_data_base(0x00500000)
        buf = st.map(0x30000010, bytearray(8))
        r = st.call("Random::next", [0x30000010])
        assert r.has_st0 and bytes(buf)[:4] == (0x3039).to_bytes(4, "little")
        with pytest.raises(L.SgError) as e:
            st.map(0x30000014, bytearray(8))
        assert e.value.code == L.SG_E_MAP
        with pytest.raises(L.SgError) as e:
            st.call("Random::next", [0x30000016])
        assert e.value.code == L.SG_E_FAULT
        ro = st.map(0x31000000, (L.C.c_uint8 * 4)(), readonly=True)
        with pytest.raises(L.SgError) as e:
            st.call("Random::next", [0x31000000])
        assert e.value.code == L.SG_E_FAULT and bytes(ro) == b"\0" * 4
        with pytest.raises(L.SgError) as e:
            st.call("no such function", [])
        assert e.value.code == L.SG_E_NO_FUNCTION


def test_control_word_is_applied_and_restored(lib32):
    with lib32.state() as st:
        for cw in (CW_NEAREST_53, CW_NEAREST_64, CW_CHOP_64, CW_DOWN_64, CW_UP_64, CW_NEAREST_24):
            lib32.set_fpcw(cw)
            assert st.call("x87Probe::controlWord", []).eax == cw
            assert st.last_fpu == (cw, 0xFFFF), "the call must leave the x87 stack empty and the word as set"
        lib32.set_fpcw(CW_NEAREST_53)
    assert lib32.set_fpcw(CW_CHOP_64) == L.SG_OK


def test_x64_refuses_what_win32_runs(lib64):
    assert lib64.set_fpcw(CW_NEAREST_53) == L.SG_OK
    for cw in (CW_NEAREST_64, CW_CHOP_64, CW_DOWN_64):
        assert lib64.set_fpcw(cw) == L.SG_E_UNSUPPORTED
    lib64.set_fpcw(CW_NEAREST_53)


# ------------------------------------------------------------------------------------------- the x87 probe
WORDS = [CW_NEAREST_53, CW_NEAREST_64, CW_CHOP_64, CW_DOWN_64, CW_UP_64, CW_NEAREST_24]


def test_probe_matches_the_x87_reference_on_win32(lib32):
    """Bit-exact against the exact-rational x87 model, every control word; the words give different results."""
    triples = _triples()
    seen = {}
    with lib32.state() as st:
        for cw in WORDS:
            lib32.set_fpcw(cw)
            got = [st.call("x87Probe::mulDiv", _slots(*t)).st0 for t in triples]
            exp = [mul_div_ref(*t, cw) for t in triples]
            bad = [(t, g, e) for t, g, e in zip(triples, got, exp) if struct.pack("<d", g) != struct.pack("<d", e)]
            assert not bad, f"cw 0x{cw:04x}: {bad[:2]}"
            seen[cw] = got
        lib32.set_fpcw(CW_NEAREST_53)
    for cw in WORDS[1:]:
        assert seen[cw] != seen[CW_NEAREST_53], f"0x{cw:04x} gave the 0x027f results on every triple (no spread)"


def test_probe_on_x64_equals_win32_only_at_the_default_word(lib32, lib64):
    triples = _triples()
    with lib32.state() as s32, lib64.state() as s64:
        lib32.set_fpcw(CW_NEAREST_53)
        lib64.set_fpcw(CW_NEAREST_53)
        for t in triples:
            assert s32.call("x87Probe::mulDiv", _slots(*t)).st0 == s64.call("x87Probe::mulDiv", _slots(*t)).st0
        lib32.set_fpcw(CW_CHOP_64)
        # x64 cannot even be asked: SSE2 doubles give the 0x027f results whatever the word
        wins = [s32.call("x87Probe::mulDiv", _slots(*t)).st0 for t in triples]
        sse = [s64.call("x87Probe::mulDiv", _slots(*t)).st0 for t in triples]
        assert wins != sse
        lib32.set_fpcw(CW_NEAREST_53)


@pytest.mark.parametrize("cw", WORDS[1:])
def test_probe_vectors_green_on_win32_and_unjudgeable_on_x64(lib32, lib64, cw):
    triples = _triples(16, seed=cw)
    doc = _doc(cw, triples, [mul_div_ref(*t, cw) for t in triples])
    r32 = V.VectorRunner(lib32, map_exe=False).run_doc(doc)
    assert r32.verdict == "GREEN" and r32.run == 16 and r32.distinct >= 8, r32.summary()
    r64 = V.VectorRunner(lib64, map_exe=False).run_doc(doc)
    assert r64.run == 0 and r64.skipped[f"fpcw 0x{cw:04x} unsupported"] == 16, r64.summary()


@pytest.mark.parametrize("cw", WORDS[1:])
def test_wrong_rounding_body_reads_red_on_win32(lib32, cw):
    """Negative control: plain C doubles (a body written without the wide intermediate / rounding control)."""
    triples = _triples(16, seed=cw)
    doc = _doc(cw, triples, [mul_div_ref(*t, cw) for t in triples])
    vr = V.VectorRunner(lib32, map_exe=False)
    assert lib32.negctl("x87Probe::mulDiv", 1) == 0
    r = vr.run_doc(doc)
    assert r.verdict == "RED" and r.failed > 0, f"wrong body read {r.summary()}"
    lib32.negctl_reset()
    assert vr.run_doc(doc).verdict == "GREEN"


def test_wrong_rounding_body_is_blind_at_the_default_word(lib32):
    """The control is an equivalent mutant at 0x027f: such vectors cannot see a rounding mistake (README, x87)."""
    triples = _triples(16)
    doc = _doc(CW_NEAREST_53, triples, [mul_div_ref(*t, CW_NEAREST_53) for t in triples])
    lib32.negctl("x87Probe::mulDiv", 1)
    assert V.VectorRunner(lib32, map_exe=False).run_doc(doc).verdict == "GREEN"


# ------------------------------------------------------------------------------------------- every function, both backends
def _registered():
    try:
        return sorted(f for f, i in L.load(L.B.dll_path("x64")).functions().items() if i["module"] != "x87probe")
    except Exception:          # noqa: BLE001
        return []


@need_corpus
@pytest.mark.parametrize("function", _registered())
def test_vectors_identical_on_both_backends(lib32, lib64, function):
    r64 = V.VectorRunner(lib64).run_function(function)
    r32 = V.VectorRunner(lib32).run_function(function)
    if not r64.vec_file:
        pytest.skip(f"{function}: no vector file")
    print("x64  ", r64.summary(), "\nwin32", r32.summary())
    assert r64.failed == 0 and r32.failed == 0, (r64.summary(), r32.summary())
    assert (r32.run, r32.passed, r32.ok, r32.distinct) == (r64.run, r64.passed, r64.ok, r64.distinct), (r64.summary(), r32.summary())
    # Win32 may judge vectors x64 skipped for an unsupported control word, never fewer
    assert r32.run >= r64.run
    assert {k: v for k, v in r32.skipped.items() if "fpcw" not in k} == {k: v for k, v in r64.skipped.items() if "fpcw" not in k}


@need_corpus
def test_existing_negctls_read_red_on_win32(lib32):
    """Random::range's wrong bodies are judged on the Win32 backend like on x64."""
    vr = V.VectorRunner(lib32)
    for mode in (1, 2, 3):
        lib32.negctl("Random::range", mode)
        r = vr.run_function("Random::range")
        assert r.verdict == "RED", (mode, r.summary())
    lib32.negctl_reset()
    assert vr.run_function("Random::range").verdict == "GREEN"


# ------------------------------------------------------------------------------------------- segments through the runner
def _accepted():
    try:
        return [r for r in S.manifest_rows() if r["status"] == "accepted"][:12]
    except Exception:          # noqa: BLE001
        return []


@need_segments
@pytest.mark.parametrize("row", _accepted(), ids=[f"{r['scenario']}/{int(r['segment']):02d}" for r in _accepted()])
def test_segment_load_check_through_the_runner(lib32, row):
    d = S.segment_dir(row)
    if not d.exists():
        pytest.skip(f"{d} not present")
    r = S.SegmentRunner(lib32).run(S.load_segment(d))
    assert r.status in ("LOADED_ONLY", "PASS"), f"{r.name}: {r.status} {r.detail}"


def test_class_hashes_equal_on_both_backends(lib32, lib64):
    import numpy as np
    img = np.random.default_rng(319).integers(0, 256, lib64.state_size, dtype=np.uint8).tobytes()
    with lib32.state() as a, lib64.state() as b:
        a.load_image(img)
        b.load_image(img)
        for cls in ("sim", "pointer", "ui", "const", "hidden", "render", "audio", "engine"):
            assert a.hash_class(cls) == b.hash_class(cls), cls
        assert a.image() == img


def test_per_vector_overhead_is_reported(lib32, lib64, capsys):
    """Informational: seconds per call of a trivial function and per vector of a full run, both backends."""
    out = []
    for name, lib in (("x64", lib64), ("win32", lib32)):
        with lib.state() as st:
            t = time.perf_counter()
            for _ in range(300):
                st.call("Random::next", [0x20000000])
            per_call = (time.perf_counter() - t) / 300
        t = time.perf_counter()
        r = V.VectorRunner(lib).run_function("Random::range")
        out.append(f"{name}: {per_call * 1e3:.3f} ms/bare call, {(time.perf_counter() - t) / max(r.run, 1) * 1e3:.2f} ms/vector")
    with capsys.disabled():
        print("\n" + "; ".join(out))
