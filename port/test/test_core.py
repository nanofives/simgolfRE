"""Phase 3.0 core: negative controls of every ported function, the defect switches, exe constants, game files, the
C++ unit tests (portsim_core_test) and the generated descriptor table (port/CORE.md)."""
from __future__ import annotations

import os
import pathlib
import struct
import subprocess
import sys

import pytest

from port.test import build as B
from port.test import lib as L
from port.test import vectors as V

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

need_corpus = pytest.mark.skipif(not VL.VEC_DIR.exists(), reason="vector corpus not present (port/corpus/vectors, local)")
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")

# (function, mode, what): wrong bodies the vectors must see (RED)
NEGCTL_RED = [
    ("sign", 1, "0 maps to 1"),
    ("approxDistance", 1, "halves with a floor shift"), ("approxDistance", 2, "max + min / 2"),
    ("direction8", 1, "axis test >="), ("direction8", 2, "mixed quadrants swapped"),
    ("directionOf", 1, "tie goes horizontal"),
    ("angleFixed", 1, "coefficient 10"), ("angleFixed", 2, "x == 0 axis results swapped"),
    ("sinScaled", 1, "no interpolation"), ("sinScaled", 2, "middle amp range shift 16 + 0"),
    ("foldRange", 1, "0x40000000 - a"), ("absDiff", 1, "phase 0x3fffffff"), ("bucketValue", 1, "18 -> -1"),
    ("cosScaled", 1, "phase 0x40000000"),
    ("decaySum", 1, "x >> 3"), ("decaySum2", 1, "y step 0x7f"), ("tileDistance", 1, "25 / 1000"),
    ("tileType", 1, "transposed"), ("tileByte", 1, "sign-extended"), ("tileFlag20", 1, "bit 0x10"),
    ("tileBlocked", 1, "x == 50 allowed"), ("typeAtPos", 1, "shift 9"), ("overlayCharAt", 1, "walk >= 0"),
    ("wallHeight", 1, "no neighbour fallback"), ("wallHeight", 2, "dirDx / dirDy swapped"),
    ("objectAt", 1, "no level extension"), ("objectAt", 2, "right edge inclusive"),
    ("inRange", 1, "courseType 1 type 0xf lo 0x32"), ("inRange", 2, "no RNG draw for type 0x10"),
    ("swapTableEntry", 1, "direct swap, scratch untouched"), ("swapTableEntry", 2, "0x3f dwords"),
    ("appendClubName", 1, "clubs 11 / 12 swapped"), ("appendClubName", 2, "strcpy"),
    ("sanitizeFileName", 1, "keeps one trailing space"), ("sanitizeFileName", 2, "empty result returns 1"),
]
# wrong bodies the vectors CANNOT see (recorded blind spots: the segments must cover them, PHASE23_PLAN.md 8.6)
NEGCTL_BLIND = [
    ("decaySum", 2, "loop bound 0x41"),
    ("tileType", 2, "zero-extended: no negative type byte in the vectors"),
    ("wallHeight", 3, "mask zero-extended: needs dir >= 8 with a negative mask byte"),
    ("objectAt", 3, "type 7 extended: no type-7 object with level != 1 in the vectors"),
]
PHASE30 = sorted({f for f, _, _ in NEGCTL_RED} | {"staticInit_gRngZero", "initSinTable"})
SEGMENTS = ROOT / "port" / "corpus" / "segments"


@pytest.mark.port
@need_corpus
@pytest.mark.parametrize("function,mode,what", NEGCTL_RED)
def test_negctl_reads_red(portlib, vrunner, function, mode, what):
    assert portlib.negctl(function, mode) == 0
    r = vrunner.run_function(function)
    assert r.verdict == "RED" and r.failed > 0, f"{function} mode {mode} ({what}) read {r.summary()}"
    portlib.negctl_reset()
    assert vrunner.run_function(function).verdict == "GREEN"


@pytest.mark.port
@need_corpus
@pytest.mark.parametrize("function,mode,what", NEGCTL_BLIND)
def test_negctl_blind_spots_are_known(portlib, vrunner, function, mode, what):
    """A blind spot that starts reading RED means the corpus grew: move it to NEGCTL_RED and update port/CORE.md."""
    portlib.negctl(function, mode)
    assert vrunner.run_function(function).verdict == "GREEN", f"{function} mode {mode} ({what}) is no longer blind"


@pytest.mark.port
def test_every_phase30_function_has_a_negctl(portlib):
    funcs = portlib.functions()
    for f in PHASE30:
        assert f in funcs, f
        assert portlib.negctl(f, 0) == 0, f      # raises when the function has no switch


# ------------------------------------------------------------------------------------------- staticInit (no vectors)
@pytest.mark.port
@pytest.mark.parametrize("mode", [0, 1])
def test_static_init_rng_zero(portlib, mode):
    """No vector exists (a static initialiser runs before any capture): g_rng becomes 0 and nothing else changes."""
    off = next(o for n, o, s in portlib.objects() if n == "g_rng")
    portlib.negctl("staticInit_gRngZero", mode)
    with portlib.state() as st:
        st.write(off, struct.pack("<I", 0x1D467741))
        st.write(off + 4, b"\x5a")
        before = bytearray(st.image())
        st.call("staticInit_gRngZero", [])
        after = bytearray(st.image())
    before[off:off + 4] = struct.pack("<I", 0)
    assert (after == before) == (mode == 0)


# ------------------------------------------------------------------------------------------- error codes
@pytest.mark.port
def test_no_return_is_reported(portlib):
    """decaySum with shift 7 and x = 0x40: x >> 7 == 0, so the original loops forever; the port says so."""
    attrs = next(o for n, o, s in portlib.objects() if n == "g_tileTypeAttrs")
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(attrs + 0x30 + 0x21, b"\x07")
        with pytest.raises(L.SgError) as e:
            st.call("decaySum", [0x40])
        assert e.value.code == L.SG_E_NO_RETURN
        assert st.call("decaySum", [0x3F]).eax == 7        # one step then x < 0x40
        st.write(attrs + 0x30 + 0x21, b"\x03")
        assert st.call("decaySum", [0x40]).eax != 0


# ------------------------------------------------------------------------------------------- defects
@pytest.mark.port
def test_defect_table(portlib):
    d = portlib.defects()
    assert "sinScaled.readPastTable" in d and d["sinScaled.readPastTable"]["ref"] == "model_defects#10"
    assert all(v["on"] == 1 for v in d.values()), "every switch defaults to the original's behaviour"
    with pytest.raises(L.SgError):
        portlib.defect("no such defect", False)


@pytest.mark.port
@need_corpus
def test_defect_sin_past_table(portlib, vrunner):
    """ON reproduces the original (GREEN on the vectors); OFF changes exactly the index-255 results."""
    assert vrunner.run_function("sinScaled").verdict == "GREEN"
    assert portlib.defect("sinScaled.readPastTable", False) == 1
    r = vrunner.run_function("sinScaled")
    assert r.verdict == "RED", "no vector reaches index 255 with a non-zero last entry"
    for f in r.failures:
        doc = VL.read_vec(V.vec_path("sinScaled"))
        v = next(v for k in doc["keys"] if k["key"] == f.key for v in k["vectors"] if v["i"] == f.i)
        angle = v["args"][0]["int"] & 0xFFFFFFFF
        assert (angle & 0x3FFFFFFF) >> 22 == 255, f"vector {f.i}: only index 255 may change"
    portlib.defect_reset()
    assert vrunner.run_function("sinScaled").verdict == "GREEN"


# ------------------------------------------------------------------------------------------- exe constants
@pytest.mark.port
@need_exe
def test_exe_constants_load_and_fill(portlib):
    assert portlib.exe_load() == "", "the anchored golf_clean.exe must verify"
    assert portlib.exe_loaded()
    dx = struct.unpack("<8i", portlib.exe_const("f_tab_dirDx_4c2878"))
    dy = struct.unpack("<8i", portlib.exe_const("f_tab_dirDy_4c2898"))
    assert sorted(zip(dx, dy)) == sorted({(a, b) for a in (-1, 0, 1) for b in (-1, 0, 1)} - {(0, 0)})
    with pytest.raises(L.SgError) as e:
        portlib.exe_const("no such constant")
    assert e.value.code == L.SG_E_NO_RESOURCE
    objs = {n: (o, s) for n, o, s in portlib.objects()}
    with portlib.state() as st:
        st.fill_from_exe()
        o, s = objs["f_tab_dirDx_4c2878"]
        assert st.read(o, s) == portlib.exe_const("f_tab_dirDx_4c2878")
        o, s = objs["g_sinTable"]
        assert st.read(o, s) == b"\0" * s, "g_sinTable is computed at run time (bss), not read from the file"
        o, s = objs["g_golfers"]
        assert st.read(o, s) == b"\0" * s, "ConstObjects fills only class const"


@pytest.mark.port
@need_exe
@pytest.mark.parametrize("mode", [0, 1])
def test_init_sin_table(portlib, mode):
    """initSinTable (boot, no vectors) against the g_sinTable of the original's segment start images (all 190 equal);
    mode 1 is its negative control."""
    imgs = sorted(SEGMENTS.glob("*/*/start.data.bin"))
    if not imgs:
        pytest.skip("segment corpus not present (port/corpus/segments, local)")
    off, size = next((o, s) for n, o, s in portlib.objects() if n == "g_sinTable")
    want = {p.read_bytes()[off:off + size] for p in imgs[:: max(1, len(imgs) // 20)]}
    assert len(want) == 1
    assert portlib.exe_load() == ""
    portlib.negctl("initSinTable", mode)
    with portlib.state() as st:
        st.call("initSinTable", [])
        assert (st.read(off, size) == want.pop()) == (mode == 0)
        assert st.read(off + size, 4) == b"\0" * 4          # the dword sinScaled reads at index 255 stays 0


@pytest.mark.port
def test_init_sin_table_needs_the_exe(portlib, tmp_path):
    p = tmp_path / "x.exe"
    p.write_bytes(b"MZ" + b"\0" * 100)
    assert portlib.exe_load(p) != ""
    try:
        with portlib.state() as st:
            with pytest.raises(L.SgError) as e:
                st.call("initSinTable", [])
            assert e.value.code == L.SG_E_NO_RESOURCE
    finally:
        if L.exe_path().exists():
            assert portlib.exe_load() == ""


@pytest.mark.port
@need_exe
@need_corpus
def test_exe_constants_equal_the_corpus_seeds(portlib):
    """The const pools the vectors seed are the bytes the port reads from the user's exe."""
    portlib.exe_load()
    objs = {n: (o, s) for n, o, s in portlib.objects()}
    gs = VL.GsMap()
    checked = 0
    with portlib.state() as st:
        st.fill_from_exe()
        for fn in ("appendClubName", "sanitizeFileName", "wallHeight", "objectAt"):
            for k in VL.read_vec(V.vec_path(fn))["keys"]:
                for r in k["seed"]:
                    if r.get("sym", "").startswith("f_"):
                        h = bytes.fromhex(r["hex"])
                        assert st.read(gs.gs_off(r["sym"], r["off"]), len(h)) == h, (fn, r["sym"], r["off"])
                        checked += 1
    assert checked > 10


@pytest.mark.port
def test_exe_refused(portlib, tmp_path):
    p = tmp_path / "not_the_exe.exe"
    p.write_bytes(b"MZ" + b"\0" * 200)
    lib = L.Lib(portlib.path) if isinstance(portlib, L.Lib) else portlib   # same DLL: the process-wide instance (the Win32 runner has one too)
    assert lib.exe_load(p) != ""
    assert not lib.exe_loaded()
    if L.exe_path().exists():
        assert lib.exe_load() == ""


# ------------------------------------------------------------------------------------------- game files
@pytest.mark.port
def test_game_files(portlib, tmp_path):
    with portlib.state() as st:
        assert st.files_size("Data/x.txt") == -1
        st.files_put("Data\\X.TXT", b"hello")
        assert st.files_size("data/x.txt") == 5
        (tmp_path / "Data").mkdir()
        (tmp_path / "Data" / "y.bin").write_bytes(b"\1\2\3")
        st.files_root(tmp_path)
        assert st.files_size("Data/y.bin") == 3 and st.files_size("data/x.txt") == -1
        st.files_root(None)
        assert st.files_size("data/x.txt") == 5


# ------------------------------------------------------------------------------------------- C++ unit tests
@pytest.mark.port
def test_core_cpp_unit_tests(portlib):
    exe = B.build_dir(B.default_arch()) / "Release" / "portsim_core_test.exe"
    if not exe.exists():
        pytest.skip("portsim_core_test not built")
    env = dict(os.environ)
    if L.exe_path().exists():
        env["SG_EXE_PATH"] = str(L.exe_path())
    r = subprocess.run([str(exe)], capture_output=True, text=True, env=env, timeout=120)
    assert r.returncode == 0, r.stdout + r.stderr


@pytest.mark.static
@need_exe
def test_execonst_desc_up_to_date():
    r = subprocess.run([sys.executable, str(ROOT / "re" / "tools" / "execonst_gen.py"), "--check"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
