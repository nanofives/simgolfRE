r"""S3.27: the original's corner without the three black-light passes (lowerEdgeCorner -> renderTileAtFiveOffsets 0x10038900).

    ~/bin/py -3.12 log/roadmap/S3/27_counterfactual.py build     # snaps/cf_k019_local_o5: localRender o5 with the black px restored
    bash log/roadmap/S3/27_glreplay_cf.sh cf_k019_local_o5         # (heavy lock) Windows GL replay -> log/p5/s327/replay/<step>.bin
    ~/bin/py -3.12 log/roadmap/S3/27_counterfactual.py compare   # box against the original and against S3.26's port frame

build: in the s327_s2b corpus, every pixel a lowerEdgeCorner pass (kind 5) changed and no localRender o0..o4 wrote is set back to
render o1's result in localRender o5's starting DIB (between render o1 and localRender o5 only those passes touch the box,
27_notes.md section 2). compare: the replayed localRender o5 against the original's own result and against the port's `course`
frame of S3.26 (26_img/host/cov_r_s2_screens.course.port.png, RGB8 from RGB555 by (v << 3) | (v >> 2), so v = c >> 3).
"""
import pathlib
import shutil
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[3]
R = ROOT / "log/p5/gl/corpus/s327_s2b/snaps"
STEP = "k019_local_o5"
BOX = (slice(451, 467), slice(781, 800))


def dib(step, w):
    return np.frombuffer((R / step / f"dib_{w}.bin").read_bytes(), "<u2").reshape(600, 800).copy()


def build():
    names = sorted(p.name for p in R.iterdir() if not p.name.startswith("cf_"))
    black = np.zeros((600, 800), bool)
    later = np.zeros((600, 800), bool)
    for n in names:
        if "_lowerEdge_" in n:
            black |= dib(n, "pre") != dib(n, "post")
        if "_local_o" in n and int(n.split("_o")[1]) < 5:
            later |= dib(n, "pre") != dib(n, "post")
    pre = dib(STEP, "pre")
    cf = pre.copy()
    sel = black & ~later
    cf[sel] = dib("k001_render_o1", "post")[sel]
    dst = R / ("cf_" + STEP)
    dst.mkdir(exist_ok=True)
    for f in (R / STEP).iterdir():
        shutil.copyfile(f, dst / f.name)
    (dst / "dib_pre.bin").write_bytes(cf.tobytes())
    print("black px", int(black.sum()), "of which later localRenders wrote", int((black & later).sum()), "restored", int(sel.sum()))


def compare():
    from PIL import Image
    cf = np.frombuffer((ROOT / "log/p5/s327/replay" / f"cf_{STEP}.bin").read_bytes(), "<u2")[:480000].reshape(600, 800)
    orig = dib(STEP, "post")
    p = np.asarray(Image.open(ROOT / "log/roadmap/S3/26_img/host/cov_r_s2_screens.course.port.png").convert("RGB")).astype(np.uint16)
    port = ((p[..., 0] >> 3) << 10) | ((p[..., 1] >> 3) << 5) | (p[..., 2] >> 3)
    print("box: cf vs orig", int((cf[BOX] != orig[BOX]).sum()), " port vs orig", int((port[BOX] != orig[BOX]).sum()),
          " cf vs port", int((cf[BOX] != port[BOX]).sum()))
    m = cf != orig
    ys, xs = np.nonzero(m)
    print("whole frame cf vs orig", int(m.sum()), "bbox x", xs.min(), xs.max(), "y", ys.min(), ys.max())
    for nm, a in (("orig", orig), ("cf  ", cf), ("port", port)):
        print("row 458", nm, " ".join(f"{v:04x}" for v in a[458, 781:800]))


if __name__ == "__main__":
    {"build": build, "compare": compare}[sys.argv[1]]()
