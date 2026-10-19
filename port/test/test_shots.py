"""Phase 3.4, the shot cycle (port/src/sim/shots.cpp): negative controls, the state tests the vectors leave open, and the
frame-overrun / missing-callee behaviour. The vectors themselves run in test_vectors.py (test_function_green_on_vectors)
and, for x87, on both backends in test_win32.py (test_vectors_identical_on_both_backends)."""
from __future__ import annotations

import pathlib
import struct
import sys

import pytest

from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = pytest.mark.port
need_corpus = pytest.mark.skipif(not VL.VEC_DIR.exists(), reason="vector corpus not present (port/corpus/vectors, local)")

# (function, mode, what): wrong bodies the vectors must see (RED)
NEGCTL_RED = [
    ("shotPower", 1, "bisection while step > 1"), ("shotPower", 2, "ring index not advanced"),
    ("wobble", 1, "middle band |s| - 9"),
    ("trialShot", 1, "air drag >> 4"), ("trialShot", 2, "bounce keeps vz / 16"),
    ("prepareShot", 1, "club clamp to 10"), ("prepareShot", 2, "power cap from reach * 3 / 4"),
    ("prepareShot", 3, "square root rounded instead of truncated (x87 path on Win32)"),
    ("chooseShotTarget", 1, "1 trial per cell in the first pass"),
    ("chooseShotTarget", 2, "variant 1 (cell corner) never tried"),
    ("scanShotArea", 1, "narrate when D > k * v"),
    ("narrateShot", 2, "off-screen exit sets hole flag bit 2"),
]
# wrong bodies the vectors cannot see (the segments must cover them, PHASE23_PLAN.md 8.6)
NEGCTL_BLIND = [
    # the aim search's scoring weights: its 12 vectors (and the prepareShot real-shot vectors that run it) end with one
    # surviving cell, so neither the per-cell score nor the pruning changes the result
    ("chooseShotTarget", 3, "hole-distance divisor always 4"),
    ("chooseShotTarget", 4, "landing value weight 4 instead of 8"),
    ("chooseShotTarget", 5, "prune tolerance starts at 0x40"),
    ("narrateShot", 1, "skill cap bucket * 2 + 5: the 2 ok vectors never reach the reward"),
]
# callees owned by other stage B children, called through the registry (MissingResource until registered)
FOREIGN = {
    "trialShot": ["heightAt42fa30", "slopeMix", "appendPairTrackPoint", "rateTile", "heightBlend"],
    "prepareShot": ["rateTile", "heightBlend", "emitGolferThought", "thoughtFlag", "typeBit7Clear",
                    "heightAt42fa30", "slopeMix", "scanTileLine"],
    "chooseShotTarget": ["rateTile", "scanTileLine", "heightAt42fa30", "slopeMix", "heightBlend",
                         "appendPairTrackPoint"],
    "scanShotArea": ["holeQuadrants"],   # its vectors that reach narrateShot stop before narrateShot's callees
    "narrateShot": ["buildGolferName", "appendHoleName", "typeBit7Clear", "postEvent", "rateTile",
                    "emitGolferThought"],
}


def _real_failures(r):
    """Failures that are not a missing resource of another child (game files / an unregistered peer of a callee)."""
    return sum(1 for f in r.failures if "missing exe constants / game file" not in f.why)


def _missing(portlib, function):
    funcs = portlib.functions()
    return [f for f in FOREIGN.get(function, []) if f not in funcs]


@need_corpus
@pytest.mark.parametrize("function,mode,what", NEGCTL_RED)
def test_negctl_reads_red(portlib, vrunner, function, mode, what):
    miss = _missing(portlib, function)
    if miss:
        pytest.skip(f"{function}: callees not registered yet: {', '.join(miss)}")
    assert portlib.negctl(function, mode) == 0
    r = vrunner.run_function(function)
    assert _real_failures(r) > 0, f"{function} mode {mode} ({what}) read {r.summary()}"
    portlib.negctl_reset()
    r = vrunner.run_function(function)
    assert _real_failures(r) == 0, r.summary()


@need_corpus
@pytest.mark.parametrize("function,mode,what", NEGCTL_BLIND)
def test_negctl_blind_spots_are_known(portlib, vrunner, function, mode, what):
    """A blind spot that starts reading RED means the corpus grew: move it to NEGCTL_RED."""
    portlib.negctl(function, mode)
    assert vrunner.run_function(function).verdict == "GREEN", f"{function} mode {mode} ({what}) is no longer blind"


def test_every_shots_function_has_a_negctl(portlib):
    funcs = portlib.functions()
    for f in ("shotPower", "wobble", "trialShot", "prepareShot", "chooseShotTarget", "scanShotArea", "narrateShot"):
        assert f in funcs, f
        assert portlib.negctl(f, 0) == 0, f


# ------------------------------------------------------------------------------------------- shotPower's cache
def _obj(portlib, name):
    return next((o, s) for n, o, s in portlib.objects() if n == name)


@pytest.mark.parametrize("hit", [True, False])
def test_shot_power_cache(portlib, hit):
    """The vectors leave the cache-hit side of the scan open (coverage partial_branch): a (dist, lie) hit returns the
    cached value and writes nothing; a dist match with another lie is a miss that fills the ring slot."""
    od, _ = _obj(portlib, "g_shotPowerCacheDist")
    ol, _ = _obj(portlib, "g_shotPowerCacheArg2")
    ov, _ = _obj(portlib, "g_shotPowerCache")
    oi, _ = _obj(portlib, "g_shotPowerRingIndex")
    with portlib.state() as st:
        st.write(od + 4 * 7, struct.pack("<i", 120))
        st.write(ol + 4 * 7, struct.pack("<i", 0x2a0))
        st.write(ov + 4 * 7, struct.pack("<i", 4321))
        st.write(oi, struct.pack("<i", 3))
        before = st.image()
        r = st.call("shotPower", [120, 0x2a0 if hit else 0x2a1, 0]).eax
        after = st.image()
        if hit:
            assert r == 4321 and after == before
        else:
            assert r != 4321
            assert struct.unpack("<i", st.read(od + 12, 4))[0] == 120
            assert struct.unpack("<i", st.read(ol + 12, 4))[0] == 0x2a1
            assert struct.unpack("<i", st.read(ov + 12, 4))[0] == r
            assert struct.unpack("<i", st.read(oi, 4))[0] == 4


def test_shot_power_ring_index_unchecked(portlib):
    """The write index is used without a bound (like the original): index 10 writes the dword after each array."""
    od, sd = _obj(portlib, "g_shotPowerCacheDist")
    oi, _ = _obj(portlib, "g_shotPowerRingIndex")
    with portlib.state() as st:
        st.write(oi, struct.pack("<i", 10))
        st.call("shotPower", [77, 0x280, 0])
        assert struct.unpack("<i", st.read(od + sd, 4))[0] == 77          # one past g_shotPowerCacheDist
        assert struct.unpack("<i", st.read(oi, 4))[0] == 1                 # (10 + 1) % 10


# ------------------------------------------------------------------------------------------- registry calls
def test_missing_callee_is_a_resource_error(portlib):
    """A callee another child has not registered yet is reported as SG_E_NO_RESOURCE naming nothing silently."""
    miss = _missing(portlib, "prepareShot")
    if not miss:
        pytest.skip("every prepareShot callee is registered")
    with portlib.state() as st:
        with pytest.raises(L.SgError) as e:
            st.call("prepareShot", [0, 0, 0x2000, 0x2000, 0])
        assert e.value.code == L.SG_E_NO_RESOURCE


# ------------------------------------------------------------------------------------------- defect switches
@need_corpus
def test_defect_shot_power_putt(portlib, vrunner):
    """shotPower.cacheIgnoresPutt (model_defects#18): ON the vectors are GREEN; OFF (putt calls bypass the cache) only
    the vectors that call with putt != 0 change."""
    from port.test import vectors as V
    assert vrunner.run_function("shotPower").verdict == "GREEN"
    portlib.defect("shotPower.cacheIgnoresPutt", 0)
    r = vrunner.run_function("shotPower")
    assert r.failed > 0
    vecs = {v["i"]: v for k in VL.read_vec(V.vec_path("shotPower"))["keys"] for v in k["vectors"]}
    assert all(vecs[f.i]["args"][2]["int"] != 0 for f in r.failures), "a non-putt vector changed"


@need_corpus
def test_defect_hazard_cell_transposed(portlib, vrunner):
    """prepareShot.hazardCellTransposed (model_defects#38): ON the vectors pass; OFF only real-shot vectors (x == -1, the
    only path with the hazard probe) change: the thought that names the cell differs."""
    from port.test import vectors as V
    if _missing(portlib, "prepareShot"):
        pytest.skip("prepareShot: callees not registered yet")
    assert _real_failures(vrunner.run_function("prepareShot")) == 0
    portlib.defect("prepareShot.hazardCellTransposed", 0)
    r = vrunner.run_function("prepareShot")
    real = [f for f in r.failures if "missing exe constants / game file" not in f.why]
    assert real, "the switch no longer changes any vector"
    keys = {k["key"]: k for k in VL.read_vec(V.vec_path("prepareShot"))["keys"]}
    for f in real:
        v = next(v for v in keys[f.key]["vectors"] if v["i"] == f.i)
        assert v["args"][2]["int"] == -1, f"{f.key} {f.i} is not a real shot"


@need_corpus
@pytest.mark.parametrize("name,function", [("chooseShotTarget.strictFlagSticky", "chooseShotTarget")])
def test_defects_the_vectors_cannot_see(portlib, vrunner, name, function):
    """model_defects#37: no vector reaches it (recorded; the segments must cover it, PHASE23_PLAN.md 8.6). Starts
    failing when the corpus grows: then turn this into a real ON / OFF test."""
    if _missing(portlib, function):
        pytest.skip(f"{function}: callees not registered yet")
    portlib.defect(name, 0)
    assert _real_failures(vrunner.run_function(function)) == 0


def _golfer(portlib, g):
    o, _ = _obj(portlib, "g_golfers")
    return o + 0x100 * g


@pytest.mark.parametrize("on", [1, 0])
def test_defect_narrate_mask_not_restored(portlib, on):
    """narrateShot.maskNotRestored (model_defects#39), on a constructed missed shot of the followed golfer whose
    skills are all 0: the pick loop finds nothing in the empty reward mask, so the call takes one of the two early
    returns. The original leaves the named golfer's mask word +0x16 at the temporary 0; fixed, it is restored."""
    if any(f not in portlib.functions() for f in ("buildGolferName", "appendHoleName", "rateTile")):
        pytest.skip("narrateShot's callees are not registered yet")
    g = 5
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        ost, _ = _obj(portlib, "g_storyPairs")
        st.write(ost, struct.pack("<h", g))                    # slot 0's golfer
        st.write(_obj(portlib, "g_playerGolfer")[0], struct.pack("<i", g))
        R = _golfer(portlib, g)
        st.write(R + 0x08, struct.pack("<ii", 0, 0x100))      # box corner x 0: off screen, followed -> no frame
        st.write(R + 0x16, struct.pack("<H", 0x1234))         # the mask word the reward saves and overwrites
        st.write(R + 0x18, b"\x01")                            # a classed golfer (the reward needs kind != 0)
        st.write(R + 0xa2, struct.pack("<h", 6))
        portlib.defect("narrateShot.maskNotRestored", on)
        st.call("narrateShot", [0, 3, 140, 1])
        mask = struct.unpack("<H", st.read(R + 0x16, 2))[0]
    assert mask == (0 if on else 0x1234)


# ------------------------------------------------------------------------------------------- x87 (Win32 backend)
@pytest.mark.x87
@need_corpus
@pytest.mark.parametrize("cw", [0x037F, 0x0F7F, 0x0B7F])
@pytest.mark.parametrize("function", ["trialShot", "prepareShot"])
def test_square_root_is_control_word_invariant(lib32, function, cw):
    """The shot cycle's own x87 code is `fild; fsqrt; __ftol` on int32 arguments (shots.cpp sqrt_ftol, inline x87 on
    Win32): at 53- and 64-bit precision its truncated root is the same under every rounding control (log/p3/p34_notes.txt
    2; 24-bit precision can differ for distances above 2896, which no vector has), so the
    vectors captured at 0x027f stay GREEN when the Win32 backend runs them under other words."""
    from port.test import vectors as V
    funcs = lib32.functions()
    if any(f not in funcs for f in FOREIGN[function]):
        pytest.skip(f"{function}: callees not registered yet")
    doc = VL.read_vec(V.vec_path(function))
    for k in doc["keys"]:
        k["fpcw"] = cw
    r = V.VectorRunner(lib32).run_doc(doc, function)
    assert r.run > 0 and _real_failures(r) == 0, r.summary()


@pytest.mark.x87
@need_corpus
def test_wrong_rounding_square_root_reads_red_on_win32(lib32):
    """prepareShot negctl 3 (the inline x87 square root converted under the current rounding control, not __ftol's
    chop) must read RED on the Win32 backend too, which proves the vectors exercise the x87 path there."""
    from port.test import vectors as V
    if any(f not in lib32.functions() for f in FOREIGN["prepareShot"]):
        pytest.skip("prepareShot: callees not registered yet")
    lib32.negctl("prepareShot", 3)
    try:
        assert _real_failures(V.VectorRunner(lib32).run_function("prepareShot")) > 0
    finally:
        lib32.negctl_reset()


def test_narrate_no_frame_starts_the_ticker(portlib):
    """narrateShot's no-frame exit (the followed golfer off screen, a good shot of an unclassed golfer: no reward) puts
    the sentence on the ticker through startMessage, which draws g_rng twice; returns 0 in the port (U-0883)."""
    if any(f not in portlib.functions() for f in ("buildGolferName", "appendHoleName")):
        pytest.skip("narrateShot's callees are not registered yet")
    g = 5
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(_obj(portlib, "g_storyPairs")[0], struct.pack("<h", g))
        st.write(_obj(portlib, "g_playerGolfer")[0], struct.pack("<i", g))
        R = _golfer(portlib, g)
        st.write(R + 0x08, struct.pack("<ii", 0, 0x100))
        st.write(R + 0xa2, struct.pack("<h", 6))
        orng, _ = _obj(portlib, "g_rng")
        st.write(orng, struct.pack("<I", 12345))
        assert st.call("narrateShot", [0, 3, 140, 0]).eax == 0
        assert struct.unpack("<I", st.read(orng, 4))[0] != 12345                       # two draws
        assert struct.unpack("<i", st.read(_obj(portlib, "g_messageTimer")[0], 4))[0] >= 16
