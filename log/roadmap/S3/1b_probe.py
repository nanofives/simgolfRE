"""S3.1b (U-1300): split a recorded draw-list pass into block categories and compare GlRaster with the REAL software GL
(re/tools/glexact/glreplay.exe) on each category alone. A category variant keeps every state call of the pass and only the
glBegin..glEnd blocks of that category, starts from the snapshot's dib_pre, and the reference is glreplay's own output for it.

    cd port/build/S3_head && SG_PORTSIM_DLL=<dll> PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/1b_probe.py g6/i012_raised_strip [...]

Categories of a block: t0 (bound texture has no image: drawn untextured), tex (textured, blending off), blend (blending on).
"""
from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys

import numpy as np

from port.test import glcorpus as G, glraster as R, lib as L

ROOT = pathlib.Path(__file__).resolve().parents[3]
REPLAY = ROOT / "re" / "tools" / "glexact" / "glreplay.exe"
SCRATCH = pathlib.Path(os.environ.get("S31B_SCRATCH", ROOT / "port" / "build" / "S3_1b_probe"))
W, H = 800, 600


def rgb(a):
    return np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=1).astype(int)


def blocks(lines, images: set):
    """(start, end, category) per glBegin..glEnd block; state is tracked through the pass."""
    out, bound, blend, tex2d = [], 0, False, True
    i = 0
    while i < len(lines):
        ln = lines[i]
        n = ln.split(" ", 1)[0]
        if n == "glBindTexture":
            bound = int(ln.split(" ", 1)[1].split(",")[1], 16)
        elif n in ("glEnable", "glDisable"):
            cap = int(ln.split(" ", 1)[1].split(",")[0], 16)
            if cap == 0x0BE2:
                blend = n == "glEnable"
            elif cap == 0x0DE1:
                tex2d = n == "glEnable"
        elif n == "glBegin":
            j = i
            while lines[j].split(" ", 1)[0] != "glEnd":
                j += 1
            cat = "blend" if blend else ("tex" if (tex2d and bound in images) else "t0")
            out.append((i, j, cat))
            i = j
        i += 1
    return out


def images_in_state(setup: R.Setup, upto: int) -> set:
    """Texture names that hold an image at the pass start (glTexImage2D after a bind, deletes honoured)."""
    have, bound = set(), 0
    for ln in setup.lines[setup.ctx_line + 1:upto]:
        n = ln.split(" ", 1)[0]
        if n == "glBindTexture":
            bound = int(ln.split(" ", 1)[1].split(",")[1], 16)
        elif n == "glTexImage2D" and "tex=" in ln:
            h = ln.split("tex=")[1].split()[0]
            if h in setup.textures:
                have.add(bound)
        elif n == "glDeleteTextures" and "ids=" in ln:
            for t in ln.split("ids=")[1].split()[0].split(","):
                have.discard(int(t, 16))
    return have


def make_root(label: str) -> pathlib.Path:
    src = G.CORPUS / label
    dst = SCRATCH / label
    (dst / "snaps").mkdir(parents=True, exist_ok=True)
    if not (dst / "stream.gl.txt").exists():
        shutil.copyfile(src / "stream.gl.txt", dst / "stream.gl.txt")
    if not (dst / "tex").exists():
        subprocess.run(["cmd", "/c", "mklink", "/J", str(dst / "tex"), str(src / "tex")], check=True, capture_output=True)
    return dst


def variant(label: str, step: str, name: str, keep, lines_override=None) -> pathlib.Path:
    """Write snaps/<step>__<name> with only the blocks keep(cat, k) is true for; returns the snapshot dir."""
    src = G.CORPUS / label / "snaps" / step
    root = make_root(label)
    d = root / "snaps" / f"{step}__{name}"
    d.mkdir(parents=True, exist_ok=True)
    for f in ("meta.txt", "vtx.bin", "nrm.bin", "dib_pre.bin", "dib_post.bin"):
        shutil.copyfile(src / f, d / f)
    lines = lines_override if lines_override is not None else (src / "pass.gl.txt").read_text().splitlines()
    setup = R.Setup(G.CORPUS / label)
    meta = G.meta(src / "meta.txt")
    bl = blocks(lines, images_in_state(setup, meta["stateIdx"]))
    drop = set()
    for k, (a, b, cat) in enumerate(bl):
        if not keep(cat, k):
            drop.update(range(a, b + 1))
    (d / "pass.gl.txt").write_text("\n".join(ln for i, ln in enumerate(lines) if i not in drop) + "\n")
    out = d / "real.bin"
    r = subprocess.run([str(REPLAY), str(root), d.name, str(out)], capture_output=True, text=True, timeout=600)
    if not out.exists():
        raise RuntimeError(r.stdout + r.stderr)
    shutil.copyfile(out, d / "dib_post.bin")
    return d


def compare(lib, label: str, step: str, want_float=False):
    setup = R.Setup(SCRATCH / label)
    res = R.run(lib, setup, step, want_float=want_float)
    post, got, st = res[0], res[1], res[2]
    dd = np.abs(rgb(post) - rgb(got)).max(axis=1)
    return post, got, st, dd, res


def main():
    lib = L.load()
    R.bind(lib.dll)
    for arg in sys.argv[1:]:
        label, step = arg.split("/")
        src = G.CORPUS / label / "snaps" / step
        setup = R.Setup(G.CORPUS / label)
        lines = (src / "pass.gl.txt").read_text().splitlines()
        imgs = images_in_state(setup, G.meta(src / "meta.txt")["stateIdx"])
        bl = blocks(lines, imgs)
        cats = {}
        for _, _, c in bl:
            cats[c] = cats.get(c, 0) + 1
        print(f"{arg}: blocks {len(bl)} {cats}")
        for name, keep in (("all", lambda c, k: True), ("t0", lambda c, k: c == "t0"), ("tex", lambda c, k: c == "tex"),
                           ("blend", lambda c, k: c == "blend"), ("notblend", lambda c, k: c != "blend")):
            variant(label, step, name, keep)
            post, got, st, dd, _ = compare(lib, label, f"{step}__{name}")
            drawn = max(st[2], 1)
            one, big = int((dd == 1).sum()), int((dd > 1).sum())
            print(f"  {name:9s} drawn {drawn:7d} one {one:6d} ({100 * one / drawn:.2f} %) big {big:5d}", flush=True)


if __name__ == "__main__":
    main()
