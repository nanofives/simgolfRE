"""Port functions against the recorded vectors of the original, with the negative controls (phase 2.6)."""
from __future__ import annotations

import copy
import pathlib
import random
import sys

import pytest

from port.test import lib as L
from port.test import vectors as V

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = pytest.mark.port
need_corpus = pytest.mark.skipif(not VL.VEC_DIR.exists(), reason="vector corpus not present (port/corpus/vectors, local)")


def _registered():
    from port.test import lib
    try:
        return sorted(lib.load().functions())
    except Exception:          # noqa: BLE001 - collection must not fail on a build error; test_library_loads reports it
        return []


def test_library_loads(portlib):
    assert portlib.state_size == 0x381174
    assert portlib.build_info().startswith("portsim ")
    assert len(portlib.objects()) == 989 and len(portlib.objects("sim")) == 203
    assert portlib.functions(), "no port function registered"


@need_corpus
@pytest.mark.parametrize("function", _registered())
def test_registry_matches_corpus(vrunner, function):
    """A registered function's abi / ret / arity must be those of every corpus key (the original's interface)."""
    p = V.vec_path(function)
    if not p.exists():
        pytest.skip(f"{function}: no vector file")
    info = vrunner.funcs[function]
    for key in VL.read_vec(p)["keys"]:
        slots = sum(2 if t in ("double", "int64", "uint64") else 1 for t in key["args"])
        assert (key["abi"], key["ret"], slots) == (info["abi"], info["ret"], info["arity"]), key["key"]


@need_corpus
@pytest.mark.parametrize("function", _registered())
def test_function_green_on_vectors(vrunner, function):
    r = vrunner.run_function(function)
    print(r.summary())
    if not r.vec_file:
        pytest.skip(f"{function}: no vector file (verified by segments only)")
    assert r.run > 0, r.summary()
    assert r.failed == 0, r.summary() + "\n" + "\n".join(f"  {f.key} {f.i}: {f.why}" for f in r.failures[:10])


# ------------------------------------------------------------------------------------------- negative controls
@need_corpus
@pytest.mark.parametrize("mode,what", [(1, "results 0 and 1 swapped"), (2, "scaled by n + 1"), (3, "rounded to nearest")])
def test_negctl_random_range_reads_red(portlib, vrunner, mode, what):
    assert portlib.negctl("Random::range", mode) == 0
    r = vrunner.run_function("Random::range")
    assert r.verdict == "RED" and r.failed > 0, f"wrong body ({what}) read {r.summary()}"
    portlib.negctl_reset()
    assert vrunner.run_function("Random::range").verdict == "GREEN"


@need_corpus
def test_corrupted_vector_reads_red(vrunner):
    doc = VL.read_vec(V.vec_path("Random::range"))
    rng = random.Random(26)
    for kind in ("out", "ret"):
        d = copy.deepcopy(doc)
        v = rng.choice([v for k in d["keys"] for v in k["vectors"] if v["status"] == "ok" and v["out"]])
        if kind == "out":
            h = bytearray.fromhex(v["out"][0]["hex"])
            h[rng.randrange(len(h))] ^= 0x01
            v["out"][0]["hex"] = h.hex()
        else:
            v["ret"] = v["ret"] + 1
        r = vrunner.run_doc(d)
        assert r.verdict == "RED" and r.failed == 1, f"{kind}: {r.summary()}"


@need_corpus
def test_extra_write_reads_red(portlib, vrunner):
    """A vector whose expected out region is dropped: the port's (correct) write is then an extra write."""
    d = copy.deepcopy(VL.read_vec(V.vec_path("Random::next")))
    v = next(v for k in d["keys"] for v in k["vectors"] if v["status"] == "ok" and v["out"])
    v["out"] = []
    r = vrunner.run_doc(d)
    assert r.failed == 1 and "extra write" in r.failures[0].why, r.summary()


@need_corpus
def test_skips_are_reported(vrunner):
    d = copy.deepcopy(VL.read_vec(V.vec_path("Random::range")))
    vs = [v for k in d["keys"] for v in k["vectors"] if v["status"] == "ok"]
    vs[0]["status"] = "emu_error"
    vs[1]["reloc_check"] = False
    r = vrunner.run_doc(d)
    assert r.skipped["status emu_error"] == 1 and r.skipped["reloc_check false"] == 1 and r.failed == 0


@need_corpus
def test_unported_function_is_not_run(vrunner):
    r = vrunner.run_function("absDiff")
    assert r.verdict == "NOT_PORTED" and r.run == 0 and r.ok > 0


# ------------------------------------------------------------------------------------------- address space
def test_address_space(portlib):
    import ctypes as C
    with portlib.state() as st:
        st.set_data_base(0x00500000)
        with pytest.raises(L.SgError) as e:
            st.map(0x00500000 + 0x1000, bytearray(16))            # inside the GameState
        assert e.value.code == L.SG_E_MAP
        buf = st.map(0x30000010, bytearray(8))
        with pytest.raises(L.SgError):
            st.map(0x30000014, bytearray(8))                      # overlaps the first block
        with pytest.raises(L.SgError) as e:
            st.call("Random::next", [0x30000016])                 # straddles the end of the block
        assert e.value.code == L.SG_E_FAULT
        ro = st.map(0x31000000, bytearray(4), readonly=True)
        with pytest.raises(L.SgError) as e:
            st.call("Random::next", [0x31000000])                 # writes a read-only mapping
        assert e.value.code == L.SG_E_FAULT
        r = st.call("Random::next", [0x30000010])
        assert r.has_st0 and bytes(buf)[:4] == (0x3039).to_bytes(4, "little") and bytes(ro) == b"\0" * 4
        with pytest.raises(L.SgError) as e:
            st.call("Random::next", [1, 2])
        assert e.value.code == L.SG_E_ARITY
        with pytest.raises(L.SgError) as e:
            st.call("no such function", [])
        assert e.value.code == L.SG_E_NO_FUNCTION
        assert C.sizeof(buf) == 8


def test_fpcw(portlib):
    assert portlib.set_fpcw(0x027F) == L.SG_OK
    if "x64" in portlib.build_info():
        assert portlib.set_fpcw(0x037F) == L.SG_E_UNSUPPORTED   # 64-bit precision: SSE2 cannot honour it
