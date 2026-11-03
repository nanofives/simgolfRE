"""S3.31: the C++ body (overlay.rasterLine, a build's portsim_c.dll) against the real GL and the Python models on S3.24's / S3.31's
record sets: per set, differing px of rule 2 and rule 8 (and control 12) against the real GL, rule 8's newly wrong / fixed pixels
against rule 2, and whether the C++ rule 8 equals 31_score.py's Python f32c:span_dir3 model pixel for pixel; the same for
rule 9 (rule 8 + the second-vertex holes) against rule 8 and 31_holes.py's model, and control 13 (no holes).

    cd port/build/S3_31 && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/31_cppcheck.py <npz>... [--limit N]
"""
import argparse
import ctypes as C
import importlib.util
import pathlib
import struct
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import lib as L  # noqa: E402

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s331s", HERE / "31_score.py")
S = importlib.util.module_from_spec(spec)
spec.loader.exec_module(S)
P = S.P
spech = importlib.util.spec_from_file_location("s331h", HERE / "31_holes.py")
Hm = importlib.util.module_from_spec(spech)
_argv = sys.argv
sys.argv = sys.argv[:1]
spech.loader.exec_module(Hm)
sys.argv = _argv
W, H, BUF = 800, 600, 0x10000000


CBUF = (C.c_uint16 * (W * H))()


def cpp(lib, st, r, rule, mode=0):
    buf = np.zeros((H, W), "<u2")
    x0, y0, x1, y1 = r["box"]
    buf[y0:y1 + 1, x0:x1 + 1] = r["pre"]
    C.memmove(CBUF, buf.tobytes(), W * H * 2)
    cbuf = CBUF
    prev = lib.negctl("overlay.rasterLine", mode)
    try:
        st.call("overlay.setRule", [rule])
        wb = struct.unpack("<I", struct.pack("<f", r["lw"]))[0]
        st.call("overlay.rasterLine", [BUF, W, H, r["x1"] & 0xFFFFFFFF, r["y1"] & 0xFFFFFFFF, r["x2"] & 0xFFFFFFFF, r["y2"] & 0xFFFFFFFF,
                                       r["colour"], r["width"] & 0xFFFFFFFF, r["alpha"] & 0xFFFFFFFF, 1 if r["flip"] else 0, wb])
    finally:
        lib.negctl("overlay.rasterLine", prev or 0)
    out = np.frombuffer(bytes(cbuf), "<u2").reshape(H, W)
    return out[y0:y1 + 1, x0:x1 + 1].copy()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("npz", nargs="+")
    ap.add_argument("--limit", type=int, default=0)
    a = ap.parse_args()
    lib = L.load()
    with lib.state() as st:
        st.map(BUF, CBUF)
        prev_rule = st.call("overlay.setRule", [8]).eax
        for p in a.npz:
            recs = P.load(p)
            if a.limit:
                recs = recs[:a.limit]
            t = dict(rec=0, r2=0, r8=0, c12=0, new=0, fixed=0, py_differ=0, rec_py_differ=0, r2_vs_py_snap=0, r9=0, c13=0,
                     new9=0, fixed9=0, py9_differ=0)
            for r in recs:
                if r["sw"] != W or r["sh"] != H or r["fw"] != W:
                    continue
                t["rec"] += 1
                c2, c8, c12 = cpp(lib, st, r, 2), cpp(lib, st, r, 8), cpp(lib, st, r, 8, 12)
                w2, w8 = P.diff5(c2, r["real"]) > 0, P.diff5(c8, r["real"]) > 0
                t["r2"] += int(w2.sum())
                t["r8"] += int(w8.sum())
                t["c12"] += int((P.diff5(c12, r["real"]) > 0).sum())
                t["new"] += int((w8 & ~w2).sum())
                t["fixed"] += int((w2 & ~w8).sum())
                py8 = S.model(r, "f32c:span_dir3")
                dd = int((py8 != c8).sum())
                t["py_differ"] += dd
                t["rec_py_differ"] += int(dd > 0)
                t["r2_vs_py_snap"] += int((S.model(r, "snap:span_dir3") != c2).sum())
                c9, c13 = cpp(lib, st, r, 9), cpp(lib, st, r, 9, 13)
                w9 = P.diff5(c9, r["real"]) > 0
                t["r9"] += int(w9.sum())
                t["c13"] += int((P.diff5(c13, r["real"]) > 0).sum())
                t["new9"] += int((w9 & ~w8).sum())
                t["fixed9"] += int((w8 & ~w9).sum())
                t["py9_differ"] += int((Hm.model(r, "lt_gt_1e-9") != c9).sum())
            print(pathlib.Path(p).name, t, flush=True)
        st.call("overlay.setRule", [prev_rule])


if __name__ == "__main__":
    main()
