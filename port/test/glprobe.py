"""Probes of Windows' generic OpenGL against the port's rasteriser, one pipeline stage at a time (phase 5 step 3).

A probe builds a small synthetic scene in the recorder's text form (state stream + pass), runs it through the REAL software GL
(re/tools/glexact/glreplay.exe, which writes the DIB it produced) and through GlRaster (the sg_raster_* C ABI), and compares the
two frames pixel by pixel. Because the scene is chosen to exercise one stage (a flat colour, a colour ramp, a texture at a known
scale, one lit triangle) a difference points at that stage.

    ~/bin/py -3.12 -m port.test.glprobe flat|ramp|tex|lit        needs re/tools/glexact/glreplay.exe (build.bat)
"""
from __future__ import annotations

import pathlib
import struct
import subprocess
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import glcorpus as G  # noqa: E402
from port.test import glraster as R  # noqa: E402
from port.test import lib as L  # noqa: E402

W, H = 800, 600
REPLAY = ROOT / "re" / "tools" / "glexact" / "glreplay.exe"
PROBE = G.CORPUS / "probe"
K = np.array([[15, 7, 13, 5], [3, 11, 1, 9], [12, 4, 14, 6], [0, 8, 2, 10]])


def fb(x: float) -> str:
    return "0x%x" % struct.unpack("<I", struct.pack("<f", x))[0]


def db(x: float) -> str:
    b = struct.unpack("<Q", struct.pack("<d", x))[0]
    return "0x%x,0x%x" % (b & 0xFFFFFFFF, b >> 32)


class Scene:
    """State lines (everything before the pass) and pass lines, in the recorder's form."""

    def __init__(self):
        self.state = ["wglCreateContext 0x1"]
        self.pass_ = []
        self.textures: dict[str, bytes] = {}
        self._tex_next = 1

    def setup2d(self, scale: float = 1.0):
        s = self.state
        s += ["glViewport 0x0,0x0,0x%x,0x%x" % (W, H), "glMatrixMode 0x1701", "glLoadIdentity",
              "glOrtho " + ",".join([db(0), db(W * scale), db(0), db(H * scale), db(-1), db(1)]), "glMatrixMode 0x1700", "glLoadIdentity"]

    def texture(self, w: int, h: int, data: bytes, fmt: int = 0x1907, mag: int = 0x2600, mini: int = 0x2601, env_ok: bool = True) -> int:
        """Upload a texture object (id returned, already bound)."""
        tid = self._tex_next
        self._tex_next += 1
        key = "0x%08x" % (0x1000 + tid)
        self.textures[key] = data
        s = self.state
        s += ["glGenTextures 0x1,0x0 ids=0x%x" % tid, "glBindTexture 0xde1,0x%x" % tid,
              "glTexParameteri 0xde1,0x2800,0x%x" % mag, "glTexParameteri 0xde1,0x2801,0x%x" % mini,
              "glTexParameteri 0xde1,0x2802,0x2901", "glTexParameteri 0xde1,0x2803,0x2901"]
        if env_ok:
            s.append("glTexEnvi 0x2300,0x2200,0x2100")   # GL_MODULATE through the valid target
        s.append("glTexImage2D 0xde1,0x0,0x%x,0x%x,0x%x,0x0,0x%x,0x1401,0x0 tex=%s" % (3 if fmt == 0x1907 else 4, w, h, fmt, key))
        return tid

    def tri(self, a, b, c, colors=None, texcoords=None):
        """A triangle from three (x, y) points (z = 0); colors / texcoords per vertex when given."""
        p = self.pass_
        p.append("glBegin 0x4")
        for i, (x, y) in enumerate((a, b, c)):
            if colors:
                p.append("glColor4f " + ",".join(fb(v) for v in colors[i]))
            if texcoords:
                p.append("glTexCoord2f %s,%s" % (fb(texcoords[i][0]), fb(texcoords[i][1])))
            p.append("glVertex3f %s,%s,%s" % (fb(x), fb(y), fb(0.0)))
        p.append("glEnd")

    def quad(self, x0, y0, x1, y1, colors=None, texcoords=None):
        """Two counter-clockwise triangles (culling stays off in the probes)."""
        self.tri((x0, y0), (x1, y0), (x1, y1), colors and [colors[0], colors[1], colors[2]], texcoords and [texcoords[0], texcoords[1], texcoords[2]])
        self.tri((x0, y0), (x1, y1), (x0, y1), colors and [colors[0], colors[2], colors[3]], texcoords and [texcoords[0], texcoords[2], texcoords[3]])

    def write(self, name: str):
        d = PROBE / name
        (d / "snaps" / "s1").mkdir(parents=True, exist_ok=True)
        (d / "tex").mkdir(exist_ok=True)
        (d / "stream.gl.txt").write_text("\n".join(self.state) + "\n")
        for h, b in self.textures.items():
            (d / "tex" / f"{h}.bin").write_bytes(b)
        sd = d / "snaps" / "s1"
        (sd / "pass.gl.txt").write_text("\n".join(self.pass_) + "\n")
        (sd / "meta.txt").write_text("\n".join(["w 800", "h 600", "wb 1600", "bpp 16", "biHeight 600", "comp 3", "m0 31744", "m1 992",
                                               "m2 31", "stateIdx %d" % len(self.state)]) + "\n")
        for f in ("vtx.bin", "nrm.bin"):
            (sd / f).write_bytes(b"\0" * 12)
        (sd / "dib_pre.bin").write_bytes(b"\0" * (W * H * 2))
        (sd / "dib_post.bin").write_bytes(b"\0" * (W * H * 2))
        return d


def real_frame(d: pathlib.Path) -> np.ndarray:
    """Run the scene through Windows' software GL; the DIB it wrote becomes the snapshot's dib_post."""
    out = d / "out.bin"
    r = subprocess.run([str(REPLAY), str(d), "s1", str(out)], capture_output=True, text=True, timeout=120)
    if not out.exists():
        raise RuntimeError(r.stdout + r.stderr)
    (d / "snaps" / "s1" / "dib_post.bin").write_bytes(out.read_bytes())
    return np.frombuffer(out.read_bytes(), dtype="<u2").copy()


def port_frame(lib, d: pathlib.Path, knobs: dict | None = None) -> np.ndarray:
    setup = R.Setup(d)

    def patch(r, dll):
        if knobs:
            t = "".join(f"#tune {i} {v}\n" for i, v in knobs.items()).encode()
            dll.sg_raster_exec(r, t, len(t))
    post, got, st, unk, pre = R.run(lib, setup, "s1", patch=patch)
    return got


def rgb5(a: np.ndarray) -> np.ndarray:
    return np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=-1).astype(int)


def compare(real: np.ndarray, port: np.ndarray, mask=None) -> dict:
    d = real != port
    if mask is not None:
        d &= mask
    return {"differing_px": int(d.sum()), "of": int(mask.sum()) if mask is not None else real.size}


# ------------------------------------------------------------------------------------------------ probes
def probe_flat(lib):
    """256 flat colours (glColor4f c / 255 on a grey ramp, then each channel alone): the 5-bit write and the dither."""
    sc = Scene()
    sc.setup2d()
    for c in range(256):
        cx, cy = (c % 16) * 50, (c // 16) * 37
        v = c / 255.0
        sc.quad(cx, cy, cx + 48, cy + 36, colors=[(v, v, v, 1.0)] * 4)
    d = sc.write("flat")
    real = real_frame(d)
    port = port_frame(lib, d)
    print("flat colours:", compare(real, port))
    # per colour, the real 5-bit value statistics (a 36-row square covers every dither cell)
    R5 = rgb5(real).reshape(H, W, 3)
    bad = 0
    for c in range(256):
        cx, cy = (c % 16) * 50, (c // 16) * 37
        blk = R5[cy:cy + 36, cx:cx + 48, 0]
        pred = np.floor(c / 255.0 * 255 * 31 / 255 + (K[np.arange(cy, cy + 36)[:, None] % 4, np.arange(cx, cx + 48)[None, :] % 4] + 0.5) / 16)
        bad += int((blk != np.clip(pred, 0, 31)).sum())
    print("flat colours: real frame vs floor(c*31/255 + (k+.5)/16):", bad, "pixels differ of", 256 * 36 * 48)
    return real, port


def probe_ramp(lib, knobs=None, quiet=False):
    """Smooth-shaded triangles: random vertex colours at random (fractional) positions, no texture, no lighting."""
    rng = np.random.default_rng(1)
    sc = Scene()
    sc.setup2d()
    for gy in range(6):
        for gx in range(8):
            ox, oy = gx * 100 + 5, gy * 100 + 5
            pts = [(ox + rng.uniform(0, 30), oy + rng.uniform(0, 30)), (ox + rng.uniform(60, 90), oy + rng.uniform(0, 40)),
                   (ox + rng.uniform(20, 90), oy + rng.uniform(50, 90))]
            cols = [tuple(rng.uniform(0, 1, 3)) + (1.0,) for _ in range(3)]
            sc.tri(*pts, colors=cols)
    d = sc.write("ramp")
    real = real_frame(d)
    port = port_frame(lib, d, knobs)
    cov = (real != 0) | (port != 0)
    r5, p5 = rgb5(real), rgb5(port)
    diff = np.abs(r5 - p5).max(axis=-1)
    cov_diff = int(((real != 0) != (port != 0)).sum())
    if quiet:
        return int((real != port).sum()), cov_diff
    print("ramp: pixels drawn by either", int(cov.sum()), "differing", int((real != port).sum()), "off by one", int(((diff == 1)).sum()),
          "more than one", int((diff > 1).sum()))
    return real, port


def sweep_snap(lib):
    """Coverage and colour differences of the ramp probe for sub-pixel vertex snapping grids."""
    print("no snap:", probe_ramp(lib, None, True))
    for bits in (2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 16):
        for mode in (0, 1):
            print("snap bits", bits, "floor" if mode else "nearest", probe_ramp(lib, {6: bits, 15: mode}, True), flush=True)


def probe_tex(lib, knobs=None, quiet=False, env=0x1E01, scales=None, name="tex", seed=3, w=90.0, world=1.0):
    """A random 64x64 texture on screen-aligned quads at several texel-per-pixel scales and sub-pixel offsets, GL_REPLACE (the
    texture alone): the sampling and filtering stage, no lighting."""
    rng = np.random.default_rng(seed)
    data = rng.integers(0, 256, 64 * 64 * 3, dtype=np.uint8).tobytes()
    sc = Scene()
    sc.setup2d(world)
    tid = sc.texture(64, 64, data)
    sc.state.append("glEnable 0xde1")
    sc.state.append("glTexEnvi 0x2300,0x2200,0x%x" % env)
    scales = scales or [0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0]
    k = 0
    for gy in range(6):
        for gx in range(8):
            sc_ = scales[k % len(scales)]
            k += 1
            ox, oy = gx * 100 + 3 + rng.uniform(0, 1), gy * 100 + 3 + rng.uniform(0, 1)
            # texel-per-pixel = sc_: the quad spans w * sc_ / 64 of the texture
            span = w * sc_ / 64.0
            u0, v0 = rng.uniform(0, 1), rng.uniform(0, 1)
            sc.quad(ox * world, oy * world, (ox + w) * world, (oy + w) * world, texcoords=[(u0, v0), (u0 + span, v0), (u0 + span, v0 + span), (u0, v0 + span)],
                    colors=[(1.0, 1.0, 1.0, 1.0)] * 4)
    d = sc.write(name)
    real = real_frame(d)
    port = port_frame(lib, d, knobs)
    return report(real, port, quiet, name)


def report(real, port, quiet, name):
    cov_diff = int(((real != 0) != (port != 0)).sum())
    diff = np.abs(rgb5(real) - rgb5(port)).max(axis=-1)
    both = (real != 0) & (port != 0)
    res = (int(((real != port) & both).sum()), int(((diff > 1) & both).sum()), cov_diff, int(both.sum()))
    if not quiet:
        print(name + ": pixels drawn by both %d, differing %d, more than one level %d, coverage differences %d" % (res[3], res[0], res[1], res[2]))
    return res


def main():
    lib = L.load()
    which = sys.argv[1] if len(sys.argv) > 1 else "flat"
    {"flat": probe_flat, "ramp": probe_ramp, "snap": sweep_snap, "tex": probe_tex}[which](lib)


if __name__ == "__main__":
    main()
