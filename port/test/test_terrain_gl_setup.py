"""Phase 5 step 3 (re/PHASE5_TERRAIN_PLAN.md): the GL setup of Terrain.dll produced by the port, against the original's recorded setup.

The recorder (re/tools/gl_corpus.py) holds, in stream.gl.txt, every GL call Terrain.dll made from process start. Its first part is
the setup: the Terrain constructor's texture load WITHOUT a context (518 files, every call a no-op in the driver), initSystem (context,
state, lighting file, the texture reload with real names, viewport and projection) and initTerrain's array pointers. The port
(portsim/sim/terrain_gl_setup.h) produces that sequence from the user's own files; the call streams must be equal line for line, after
dropping what a run cannot repeat: pointer arguments, the context handles, and the fourth float of the ambient light, which the
original reads past a float[3] (garbage that differs from run to run, and does not reach a pixel).

Also compared: the texture table the setup leaves (g_textures) with the one in the Terrain.dll image of a snapshot.
Negative controls (the port's setup with one thing wrong) are in the file's second half.
"""
from __future__ import annotations

import ctypes as C
import pathlib

import pytest

from port.test import glcorpus as G
from port.test import lib as L

pytestmark = pytest.mark.port

ROOT = G.CORPUS / "c7"
INSTALL = G.ROOT / "original"
N_TABLE = 0x25 * 0x19 * 9


def normalize_setup(lines):
    """Drop pointer arguments, handles and the garbage fourth ambient float; one comparable text per call."""
    out = []
    for ln in lines:
        ln = ln.rstrip()
        if not ln or ln.startswith("#"):
            continue
        toks = ln.split(" ")
        name, rest = toks[0], toks[1:]
        args = rest[0].split(",") if rest and not rest[0].startswith(("v=", "ids=", "tex=", "p=")) else []
        extras = [t for t in rest if t.startswith(("v=", "ids=", "tex="))]
        keep = {"wglCreateContext": 0, "wglMakeCurrent": 0, "glGenTextures": 1, "glDeleteTextures": 1, "glTexImage2D": 8,
                "glLightfv": 2, "glMaterialfv": 2, "glVertexPointer": 3, "glNormalPointer": 2}.get(name)
        if keep is not None:
            args = args[:keep]
        if name == "glLightfv" and args[:2] == ["0x4000", "0x1200"] and extras and extras[0].startswith("v="):
            v = extras[0][2:].split(",")
            extras[0] = "v=" + ",".join(v[:3] + ["*"])
        out.append(name + (" " + ",".join(args) if args else "") + "".join(" " + e for e in extras))
    return out


def recorded_setup():
    """The recorded stream up to and including the first glNormalPointer (initTerrain's last call)."""
    lines = (ROOT / "stream.gl.txt").read_text().splitlines()
    end = next(i for i, ln in enumerate(lines) if ln.startswith("glNormalPointer"))
    return normalize_setup(lines[:end + 1])


@pytest.fixture(scope="module")
def lib():
    return L.load()


def run_setup(lib, install=INSTALL):
    dll = lib.dll
    dll.sg_gl_setup.restype = C.c_int
    dll.sg_gl_setup.argtypes = [C.c_void_p, C.c_char_p, C.c_size_t, C.POINTER(C.c_size_t), C.POINTER(C.c_uint32)]
    with lib.state() as st:
        st.files_root(str(install))
        cap = 1 << 25
        out = C.create_string_buffer(cap)
        n = C.c_size_t(0)
        table = (C.c_uint32 * N_TABLE)()
        code = dll.sg_gl_setup(st.ctx, out, cap, C.byref(n), table)
        if code:
            raise RuntimeError(f"sg_gl_setup {code}")
        return normalize_setup(out.raw[:n.value].decode().splitlines()), list(table)


def _need():
    if not (ROOT / "stream.gl.txt").exists():
        pytest.skip("no terrain corpus under port/corpus/gl/c7 (python -m port.test.glcorpus export log/p5/gl/corpus/c7 c7)")
    if not (INSTALL / "Data" / "Textures" / "Parkland").is_dir():
        pytest.skip("no install with Data/Textures (original/)")


def test_setup_stream_equals_the_original(lib):
    _need()
    got, _ = run_setup(lib)
    exp = recorded_setup()
    d = G.first_difference(got, exp)
    assert d is None, f"first difference at line {d[0]}: port {d[1]!r} original {d[2]!r} ({len(got)} vs {len(exp)} lines)"


def test_setup_leaves_the_original_texture_table(lib):
    _need()
    _, table = run_setup(lib)
    snap = G.Snap(ROOT, "sandbox")
    want = list(snap.arr(G.RVA["textures"], N_TABLE, C.c_uint32))
    bad = [i for i, (a, b) in enumerate(zip(table, want)) if a != b]
    assert not bad, f"{len(bad)} of {N_TABLE} table entries differ, first at {bad[0]}: port {table[bad[0]]:#x} original {want[bad[0]]:#x}"


# ---- negative controls: an install with one thing changed must not reproduce the recorded setup ----
@pytest.fixture(scope="module")
def tweaked(tmp_path_factory):
    """A copy of the Parkland texture set and its lighting file in a temporary install; returns a function that edits and runs it."""
    import shutil
    _need()
    root = tmp_path_factory.mktemp("install")
    shutil.copytree(INSTALL / "Data" / "Textures" / "Parkland", root / "Data" / "Textures" / "Parkland")
    shutil.copy(INSTALL / "ParklandLighting.txt", root / "ParklandLighting.txt")
    return root


def _fresh(tweaked, tmp_path):
    import shutil
    dst = tmp_path / "install"
    shutil.copytree(tweaked, dst)
    return dst


def test_control_the_unmodified_copy_reproduces_the_stream(lib, tweaked):
    got, _ = run_setup(lib, tweaked)
    assert got == recorded_setup()


def test_control_one_texture_byte_changes_the_stream(lib, tweaked, tmp_path):
    dst = _fresh(tweaked, tmp_path)
    f = dst / "Data" / "Textures" / "Parkland" / "TeeA0003.bmp"
    b = bytearray(f.read_bytes())
    b[200] ^= 1
    f.write_bytes(bytes(b))
    got, _ = run_setup(lib, dst)
    assert got != recorded_setup()


def test_control_the_lighting_file_changes_the_stream(lib, tweaked, tmp_path):
    dst = _fresh(tweaked, tmp_path)
    t = dst / "ParklandLighting.txt"
    t.write_text(t.read_text().replace("200 200 180", "201 200 180", 1))
    got, _ = run_setup(lib, dst)
    assert got != recorded_setup()


def test_control_a_missing_texture_changes_the_stream(lib, tweaked, tmp_path):
    dst = _fresh(tweaked, tmp_path)
    (dst / "Data" / "Textures" / "Parkland" / "TeeA0005.bmp").unlink()
    got, table = run_setup(lib, dst)
    assert got != recorded_setup() and len(got) != len(recorded_setup())
