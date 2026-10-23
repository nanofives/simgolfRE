"""Harness for the terrain renderer's replay corpus (phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md section 8).

A snapshot step of re/tools/gl_corpus.py holds, for one draw pass of the ORIGINAL Terrain.dll code: the pass's GL calls
(pass.gl.txt), the Terrain object at the pass (tobj.bin, 0x164ad0 bytes at the address meta.txt names), Terrain.dll's
.rdata + .data image (tdata.bin, RVA 0x5f000 .. 0x113000) and the exported call that started the pass (entry_kind: 2 =
Terrain::render, 3 = localRender, 4 = pathUpdateRender, 1 = stripRender; entry_a0 .. a2 its raw arguments).

This module turns one into the port's inputs: the Terrain object is mapped into a context at its original address (its tile
pointers stay valid), and TerrainGlobals is filled from the data image at the offsets below. The addresses live HERE, in the
tooling, never in port code. The port's call stream is compared line by line with the recorded one after dropping the pointer
arguments of the glXXXfv calls (the port has no such pointers; the values behind them are compared).

    ~/bin/py -3.12 -m port.test.glcorpus export log/p5/gl/corpus/c7 c7     # copy what the tests need into port/corpus/gl/c7
"""
from __future__ import annotations

import ctypes as C
import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

CORPUS = ROOT / "port" / "corpus" / "gl"
DATA_RVA0 = 0x5F000                      # tdata.bin starts at this RVA of Terrain.dll
RVA = {"side_normals": 0x63C40, "tex_coords": 0x63CA0, "pfd": 0x63E10, "textures": 0x687F8, "wall": 0x70688,
       "fence1": 0x70304, "fence0": 0x6FF80, "scale": 0x70A10, "view": 0x70A14, "normals": 0x70A18, "vertices": 0xB28C8}
N_VERTICES = 22500                       # entries of g_vertices / g_normals (the normal array spans 0x70a18 .. 0xb28c8)
N_TEXTURES = 0x25 * 0x19 * 9
N_TEXCOORD_FLOATS = (RVA["pfd"] - RVA["tex_coords"]) // 4
NEEDED = ("pass.gl.txt", "tobj.bin", "tdata.bin", "meta.txt")
RASTER = ("vtx.bin", "nrm.bin", "dib_pre.bin", "dib_post.bin")   # extra files of a snapshot for the rasteriser tests
ENTRY_RENDER = 2


class SgGlGlobals(C.Structure):
    _fields_ = [("vertices", C.c_void_p), ("n_vertices", C.c_uint32), ("normals", C.c_void_p), ("n_normals", C.c_uint32),
                ("textures", C.c_void_p), ("n_textures", C.c_uint32), ("wall_texture", C.c_uint32),
                ("fence_texture0", C.c_uint32), ("fence_texture1", C.c_uint32), ("tex_coords", C.c_void_p),
                ("n_tex_coords", C.c_uint32), ("side_normals", C.c_void_p), ("screen_scale_bits", C.c_uint32),
                ("view_index", C.c_int32)]


def bind(dll):
    """ctypes signatures of the sg_gl_* calls (kept here so lib.py's shared table stays untouched)."""
    P, U32, I32, SZ = C.c_void_p, C.c_uint32, C.c_int32, C.c_size_t
    dll.sg_gl_set_globals.restype, dll.sg_gl_set_globals.argtypes = C.c_int, [P, C.POINTER(SgGlGlobals)]
    dll.sg_gl_render.restype = C.c_int
    dll.sg_gl_render.argtypes = [P, I32, U32, U32, U32, U32, C.c_char_p, SZ, C.POINTER(SZ)]
    dll.sg_gl_view_index.restype, dll.sg_gl_view_index.argtypes = I32, [P]
    dll.sg_gl_negctl.restype, dll.sg_gl_negctl.argtypes = C.c_int, [I32]


def meta(path: pathlib.Path) -> dict[str, int]:
    out = {}
    for ln in path.read_text().splitlines():
        k, _, v = ln.partition(" ")
        if v.strip().lstrip("-").isdigit():
            out[k] = int(v)
    return out


def normalize(text: str) -> list[str]:
    """The recorder's lines in the port's form: a glXXXfv call keeps only its `v=` values (the pointer argument is dropped),
    every line loses trailing blanks, empty lines go."""
    out = []
    for ln in text.splitlines():
        ln = ln.rstrip()
        if not ln:
            continue
        toks = ln.split(" ")
        vs = [t for t in toks[1:] if t.startswith("v=")]
        out.append(toks[0] + " " + vs[0] if vs else ln)
    return out


class Snap:
    def __init__(self, root: pathlib.Path, step: str):
        d = root / "snaps" / step
        self.step = step
        self.dir = d
        self.meta = meta(d / "meta.txt")
        self.expected = normalize((d / "pass.gl.txt").read_text())
        self.tobj = (d / "tobj.bin").read_bytes()
        self.tdata = (d / "tdata.bin").read_bytes()
        self.kind = self.meta.get("entry_kind", 0)
        self.terrain = self.meta["tobjAddr"]

    def u32(self, rva: int) -> int:
        o = rva - DATA_RVA0
        return int.from_bytes(self.tdata[o:o + 4], "little")

    def arr(self, rva: int, n: int, ctype):
        o = rva - DATA_RVA0
        return (ctype * n).from_buffer_copy(self.tdata[o:o + n * C.sizeof(ctype)])


def snaps(root: pathlib.Path = CORPUS / "c7", kind: int | None = None) -> list[str]:
    if not (root / "snaps").is_dir():
        return []
    out = []
    for d in sorted((root / "snaps").iterdir()):
        if all((d / n).exists() for n in NEEDED) and (kind is None or meta(d / "meta.txt").get("entry_kind") == kind):
            out.append(d.name)
    return out


def load(lib, st, snap: Snap, textures=None) -> list:
    """Map the snapshot's Terrain object into the state `st` at its original address and hand the renderer its globals; the
    returned arrays must stay alive while the context is used."""
    bind(lib.dll)
    st.map(snap.terrain, (C.c_uint8 * len(snap.tobj)).from_buffer_copy(snap.tobj))
    g = SgGlGlobals()
    keep = [snap.arr(RVA["vertices"], N_VERTICES * 3, C.c_float), snap.arr(RVA["normals"], N_VERTICES * 3, C.c_float),
            (C.c_uint32 * N_TEXTURES)(*textures) if textures is not None else snap.arr(RVA["textures"], N_TEXTURES, C.c_uint32), snap.arr(RVA["tex_coords"], N_TEXCOORD_FLOATS, C.c_float),
            snap.arr(RVA["side_normals"], 24, C.c_float)]
    g.vertices, g.n_vertices = C.addressof(keep[0]), N_VERTICES
    g.normals, g.n_normals = C.addressof(keep[1]), N_VERTICES
    g.textures, g.n_textures = C.addressof(keep[2]), N_TEXTURES
    g.tex_coords, g.n_tex_coords = C.addressof(keep[3]), N_TEXCOORD_FLOATS
    g.side_normals = C.addressof(keep[4])
    g.wall_texture, g.fence_texture1, g.fence_texture0 = snap.u32(RVA["wall"]), snap.u32(RVA["fence1"]), snap.u32(RVA["fence0"])
    g.screen_scale_bits = snap.u32(RVA["scale"])
    g.view_index = 0
    code = lib.dll.sg_gl_set_globals(st.ctx, C.byref(g))
    if code:
        raise RuntimeError(f"sg_gl_set_globals {code}")
    return keep


def render(lib, snap: Snap, cap: int = 1 << 26) -> list[str]:
    """Run the port's Terrain::render over the snapshot; returns the call lines."""
    with lib.state() as st:
        keep = load(lib, st, snap)  # noqa: F841
        out = C.create_string_buffer(cap)
        n = C.c_size_t(0)
        a = [snap.meta.get(f"entry_a{i}", 0) for i in range(3)]
        code = lib.dll.sg_gl_render(st.ctx, snap.kind, snap.terrain, a[0], a[1], a[2], out, cap, C.byref(n))
        if code:
            raise RuntimeError(f"sg_gl_render code {code} after {n.value} bytes")
        return out.raw[:n.value].decode().splitlines()


def first_difference(a: list[str], b: list[str]):
    """(index, got, expected) of the first differing line, or None."""
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return i, x, y
    if len(a) != len(b):
        i = min(len(a), len(b))
        return i, (a[i] if i < len(a) else "<end>"), (b[i] if i < len(b) else "<end>")
    return None


def export(src: pathlib.Path, label: str) -> None:
    """Copy the files the tests need from a gl_corpus.py corpus into port/corpus/gl/<label>: per snapshot the renderer's inputs
    and the rasteriser's (arrays, back buffer before / after); in the root the state stream and the texture bytes."""
    dst = CORPUS / label
    n = 0
    for d in sorted((src / "snaps").iterdir()):
        if not all((d / f).exists() for f in NEEDED):
            continue
        (dst / "snaps" / d.name).mkdir(parents=True, exist_ok=True)
        for f in NEEDED + RASTER:
            if (d / f).exists():
                shutil.copyfile(d / f, dst / "snaps" / d.name / f)
        n += 1
    shutil.copyfile(src / "stream.gl.txt", dst / "stream.gl.txt")
    (dst / "tex").mkdir(exist_ok=True)
    for f in (src / "tex").glob("*.bin"):
        shutil.copyfile(f, dst / "tex" / f.name)
    print(f"exported {n} snapshots to {dst}")


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "export":
        export(pathlib.Path(sys.argv[2]), sys.argv[3])
    else:
        print(__doc__)
