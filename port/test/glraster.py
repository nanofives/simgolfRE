"""Harness for the software rasteriser against the recorded back-buffer frames (phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md).

For a corpus snapshot (re/tools/gl_corpus.py, exported with port.test.glcorpus): replay the state stream (everything after the
context was created, up to where the pass starts: viewport, projection, texture objects, state) into a GlRaster, put the
back buffer the original had BEFORE the pass into it, replay the pass's GL calls, and compare with the back buffer the original
had after glFlush (dib_post.bin, RGB555, 800x600, memory row = window y).

    ~/bin/py -3.12 -m port.test.glraster [c7] [step ...]       one line per step: differing pixels, where they are
"""
from __future__ import annotations

import ctypes as C
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import glcorpus as G  # noqa: E402

W, H = 800, 600


def bind(dll):
    P, SZ, I32 = C.c_void_p, C.c_size_t, C.c_int32
    dll.sg_raster_new.restype, dll.sg_raster_new.argtypes = P, [I32, I32]
    dll.sg_raster_free.restype, dll.sg_raster_free.argtypes = None, [P]
    dll.sg_raster_texture.restype, dll.sg_raster_texture.argtypes = None, [P, C.c_char_p, C.c_char_p, SZ]
    dll.sg_raster_arrays.restype, dll.sg_raster_arrays.argtypes = None, [P, C.c_char_p, SZ, C.c_char_p, SZ]
    dll.sg_raster_exec.restype, dll.sg_raster_exec.argtypes = I32, [P, C.c_char_p, SZ]
    dll.sg_raster_set_pixels.restype, dll.sg_raster_set_pixels.argtypes = None, [P, C.c_char_p, SZ]
    dll.sg_raster_get_pixels.restype, dll.sg_raster_get_pixels.argtypes = SZ, [P, C.c_char_p, SZ]
    dll.sg_raster_stats.restype, dll.sg_raster_stats.argtypes = C.c_int, [P, C.POINTER(C.c_int64)]
    dll.sg_raster_get_source.restype, dll.sg_raster_get_source.argtypes = SZ, [P, C.c_char_p, SZ]
    dll.sg_raster_keep_float.restype, dll.sg_raster_keep_float.argtypes = None, [P, I32]
    dll.sg_raster_get_float.restype, dll.sg_raster_get_float.argtypes = SZ, [P, C.c_char_p, SZ]
    dll.sg_raster_get_rho.restype, dll.sg_raster_get_rho.argtypes = SZ, [P, C.c_void_p, SZ]


class Setup:
    """The parts of a corpus root every snapshot shares: the state stream and the texture bytes (read once)."""

    def __init__(self, root: pathlib.Path):
        self.root = root
        self.lines = (root / "stream.gl.txt").read_text().splitlines()
        self.ctx_line = next(i for i, ln in enumerate(self.lines) if ln.startswith("wglCreateContext"))
        self.textures = {p.stem: p.read_bytes() for p in (root / "tex").glob("*.bin")}


def run(lib, setup: Setup, step: str, patch=None, want_float: bool = False):
    """Replay one snapshot; returns (expected, got, stats, unknown_lines). `patch(raster_handle, dll)` runs after the setup
    (negative controls)."""
    dll = lib.dll
    bind(dll)
    d = setup.root / "snaps" / step
    meta = G.meta(d / "meta.txt")
    r = dll.sg_raster_new(W, H)
    try:
        for h, b in setup.textures.items():
            dll.sg_raster_texture(r, h.encode(), b, len(b))
        state = "\n".join(setup.lines[setup.ctx_line + 1:meta["stateIdx"]]).encode()
        unknown = dll.sg_raster_exec(r, state, len(state))
        vtx, nrm = (d / "vtx.bin").read_bytes(), (d / "nrm.bin").read_bytes()
        dll.sg_raster_arrays(r, vtx, len(vtx) // 12, nrm, len(nrm) // 12)
        pre = (d / "dib_pre.bin").read_bytes()
        post = np.frombuffer((d / "dib_post.bin").read_bytes(), dtype="<u2")
        dll.sg_raster_set_pixels(r, pre, W * H)
        if want_float:
            dll.sg_raster_keep_float(r, 1)
        if patch:
            patch(r, dll)
        pas = (d / "pass.gl.txt").read_bytes()
        unknown += dll.sg_raster_exec(r, pas, len(pas))
        buf = C.create_string_buffer(W * H * 2)
        if dll.sg_raster_get_pixels(r, buf, W * H * 2) != W * H:
            raise RuntimeError("sg_raster_get_pixels")
        st = (C.c_int64 * 5)()
        dll.sg_raster_stats(r, st)
        got = np.frombuffer(buf.raw, dtype="<u2")
        if want_float:
            fb = C.create_string_buffer(W * H * 12)
            dll.sg_raster_get_float(r, fb, W * H * 12)
            sb = C.create_string_buffer(W * H * 32)
            dll.sg_raster_get_source(r, sb, W * H * 32)
            return (post, got, list(st), unknown, np.frombuffer(pre, dtype="<u2"),
                    np.frombuffer(fb.raw, dtype="<f4").reshape(H, W, 3), np.frombuffer(sb.raw, dtype="<f4").reshape(H, W, 8))
        return post, got, list(st), unknown, np.frombuffer(pre, dtype="<u2")
    finally:
        dll.sg_raster_free(r)


def run_chain(lib, setup: Setup, step: str, patch=None):
    """The whole chain: the port's Terrain::render over the snapshot's Terrain object and data drives the rasteriser directly
    (sg_gl_render_to_raster); the state stream and the starting buffer are the original's. Returns (post, got, stats, pre)."""
    dll = lib.dll
    bind(dll)
    G.bind(dll)
    dll.sg_gl_render_to_raster.restype = C.c_int
    dll.sg_gl_render_to_raster.argtypes = [C.c_void_p, C.c_void_p, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]
    snap = G.Snap(setup.root, step)
    d = setup.root / "snaps" / step
    r = dll.sg_raster_new(W, H)
    try:
        with lib.state() as st:
            keep = G.load(lib, st, snap)  # noqa: F841
            for h, b in setup.textures.items():
                dll.sg_raster_texture(r, h.encode(), b, len(b))
            state = "\n".join(setup.lines[setup.ctx_line + 1:snap.meta["stateIdx"]]).encode()
            dll.sg_raster_exec(r, state, len(state))
            pre = (d / "dib_pre.bin").read_bytes()
            post = np.frombuffer((d / "dib_post.bin").read_bytes(), dtype="<u2")
            dll.sg_raster_set_pixels(r, pre, W * H)
            if patch:
                patch(r, dll)
            a = [snap.meta.get(f"entry_a{i}", 0) for i in range(3)]
            code = dll.sg_gl_render_to_raster(st.ctx, r, snap.kind, snap.terrain, a[0], a[1], a[2])
            if code:
                raise RuntimeError(f"sg_gl_render_to_raster {code}")
            buf = C.create_string_buffer(W * H * 2)
            dll.sg_raster_get_pixels(r, buf, W * H * 2)
            stt = (C.c_int64 * 5)()
            dll.sg_raster_stats(r, stt)
            return post, np.frombuffer(buf.raw, dtype="<u2"), list(stt), np.frombuffer(pre, dtype="<u2")
    finally:
        dll.sg_raster_free(r)


def run_chain_from_setup(lib, setup: Setup, step: str, install, patch=None):
    """The chain with NO recorded state: the port's own GL setup (constructor texture load, initSystem, initTerrain) from the user's
    files drives the rasteriser, its texture table feeds the renderer, then Terrain::render draws the pass over the snapshot's Terrain
    object. Only the Terrain object, the vertex / normal arrays and the starting buffer come from the snapshot."""
    dll = lib.dll
    bind(dll)
    G.bind(dll)
    dll.sg_gl_render_to_raster.restype = C.c_int
    dll.sg_gl_render_to_raster.argtypes = [C.c_void_p, C.c_void_p, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]
    dll.sg_gl_setup_to_raster.restype = C.c_int
    dll.sg_gl_setup_to_raster.argtypes = [C.c_void_p, C.c_void_p, C.c_int32, C.POINTER(C.c_uint32)]
    snap = G.Snap(setup.root, step)
    d = setup.root / "snaps" / step
    r = dll.sg_raster_new(W, H)
    try:
        with lib.state() as st:
            st.files_root(str(install))
            table = (C.c_uint32 * G.N_TEXTURES)()
            zoom = int.from_bytes(snap.tobj[0x1C:0x20], "little", signed=True)   # Terrain +0x1c: the zoom the pass ran at
            code = dll.sg_gl_setup_to_raster(st.ctx, r, zoom, table)
            if code:
                raise RuntimeError(f"sg_gl_setup_to_raster {code}")
            keep = G.load(lib, st, snap, textures=list(table))  # noqa: F841
            pre = (d / "dib_pre.bin").read_bytes()
            post = np.frombuffer((d / "dib_post.bin").read_bytes(), dtype="<u2")
            dll.sg_raster_set_pixels(r, pre, W * H)
            if patch:
                patch(r, dll)
            a = [snap.meta.get(f"entry_a{i}", 0) for i in range(3)]
            code = dll.sg_gl_render_to_raster(st.ctx, r, snap.kind, snap.terrain, a[0], a[1], a[2])
            if code:
                raise RuntimeError(f"sg_gl_render_to_raster {code}")
            buf = C.create_string_buffer(W * H * 2)
            dll.sg_raster_get_pixels(r, buf, W * H * 2)
            stt = (C.c_int64 * 5)()
            dll.sg_raster_stats(r, stt)
            return post, np.frombuffer(buf.raw, dtype="<u2"), list(stt), np.frombuffer(pre, dtype="<u2")
    finally:
        dll.sg_raster_free(r)


# The frame criterion (re/PHASE5_TERRAIN_PLAN.md section 18): pixels one 5-bit level off at most MAX_ONE_LEVEL of the drawn pixels,
# at most MAX_BIG pixels further off. Since the rasteriser sets triangles up like the generic GL (8-fractional-bit fixed-point vertices
# of a viewport of half-size h - 2^-8), the one-level rate is 0.24 .. 1.05 % on every pass of c7 and t3 at every zoom (it no longer
# grows with the texel-per-pixel scale: that came from the earlier linear sample-position model). With the specular table and the
# tie rule of roadmap S3.1b (U-1300) the rate is 0.0 .. 0.21 % on every recorded pass (c3 .. c7, t3, g2 .. g6), at most 13 pixels
# further off; the constants stay those of the roadmap's G2 criterion.
MAX_ONE_LEVEL = 0.015
MAX_BIG = 70


def one_level_limit(median_rho: float = 0.0) -> float:
    """The one-level allowance as a fraction of the drawn pixels: MAX_ONE_LEVEL at every texel-per-pixel scale (section 18)."""
    return MAX_ONE_LEVEL


def report(post, got, pre) -> dict:
    diff = post != got
    changed = post != pre
    rgb = lambda a: np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=1).astype(int)
    dd = np.abs(rgb(post) - rgb(got)).max(axis=1)
    return {"changed": int(changed.sum()), "diff": int(diff.sum()), "diff_of_changed": int((diff & changed).sum()),
            "off_by_one": int(((dd == 1) & diff).sum()), "gt1": int(((dd > 1) & diff).sum()),
            "max": int(dd.max()) if diff.any() else 0}


def main():
    from port.test import lib as L
    args = sys.argv[1:]
    label = args.pop(0) if args and (G.CORPUS / args[0]).exists() else "c7"
    root = G.CORPUS / label
    steps = args or G.snaps(root)
    setup = Setup(root)
    lib = L.load()
    print("step\tchanged\tdiff\tbad_px_gt1\toff_by_one\tmax\ttriangles\tculled\tdrawn_px\tunknown")
    for s in steps:
        post, got, st, unknown, pre = run(lib, setup, s)
        r = report(post, got, pre)
        print(f"{s}\t{r['changed']}\t{r['diff']}\t{r['gt1']}\t{r['off_by_one']}\t{r['max']}\t{st[0]}\t{st[1]}\t{st[2]}\t{unknown}", flush=True)


if __name__ == "__main__":
    main()
