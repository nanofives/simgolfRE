"""S3.24: the game's own drawLine calls (SG_OVERLAY_DUMP of the census scenarios: every call of the run with its box's pixels
before) through the real GL, against the C++ body under line rules 0 (S3.8), 2 (default) and 6.

    ~/bin/py -3.12 log/roadmap/S3/24_streams.py real <out.npz> <dump>... [--per 1500]   (glreplay: heavy lock)
    cd port/build/S3_24 && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/24_streams.py score <out.npz>

`real` keeps up to --per distinct calls per dump (distinct by arguments and the box's pixels before), spread over the run.
"""
import argparse
import ctypes as C
import hashlib
import importlib.util
import pathlib
import struct
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("p24", HERE / "24_probe.py")
P = importlib.util.module_from_spec(spec)
spec.loader.exec_module(P)


def cmd_real(a):
    recs = []
    for d in a.dump:
        seen, lst = set(), []
        for r in P.read_dump(d):
            k = hashlib.sha1(struct.pack("<4iI3i", r["x1"], r["y1"], r["x2"], r["y2"], r["colour"], r["width"], r["alpha"],
                                         r["flip"]) + r["pre"].tobytes()).hexdigest()
            if k not in seen:
                seen.add(k)
                lst.append(r)
        step = max(1, len(lst) // a.per)
        pick = lst[::step][:a.per]
        print(pathlib.Path(d).stem, len(lst), "distinct calls,", len(pick), "kept", flush=True)
        recs += pick
    real = P.run_real(recs, "s324streams")
    P.save(a.out, recs, real)


def cmd_score(a):
    sys.path.insert(0, str(pathlib.Path.cwd()))
    from port.test import lib as L
    lib = L.load()
    recs = P.load(a.npz)
    BUF = 0x40000000
    wrong = {}
    with lib.state() as st:
        buf = (C.c_uint16 * (800 * 600))()
        st.map(BUF, buf)
        for rule in (0, 2, 6):
            st.call("overlay.setRule", [rule])
            ws = []
            for r in recs:
                x0, y0, x1, y1 = r["box"]
                fb = np.zeros((600, 800), "<u2")
                fb[y0:y1 + 1, x0:x1 + 1] = r["pre"]
                C.memmove(buf, fb.tobytes(), 800 * 600 * 2)
                wb = struct.unpack("<I", struct.pack("<f", r["lw"]))[0]
                st.call("overlay.rasterLine", [BUF, 800, 600, r["x1"], r["y1"], r["x2"], r["y2"], r["colour"], r["width"], r["alpha"],
                                               r["flip"], wb])
                got = np.frombuffer(bytes(buf), "<u2").reshape(600, 800)[y0:y1 + 1, x0:x1 + 1]
                ws.append(P.diff5(got, r["real"]))
            wrong[rule] = ws
        st.call("overlay.setRule", [2])
    changed = sum(int((r["real"] != r["pre"]).sum()) for r in recs)
    print(len(recs), "calls,", changed, "pixels the real GL changed")
    for rule in (0, 2, 6):
        one = sum(int((w == 1).sum()) for w in wrong[rule])
        beyond = sum(int((w > 1).sum()) for w in wrong[rule])
        new = sum(int(((w > 0) & (w0 == 0)).sum()) for w, w0 in zip(wrong[rule], wrong[0]))
        calls_bad = sum(int((w > 0).any()) for w in wrong[rule])
        print(f"rule {rule}: differing {one + beyond} (one level {one}, beyond {beyond}), calls with a difference {calls_bad},"
              f" newly wrong against rule 0: {new}")


def main():
    ap = argparse.ArgumentParser()
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("real")
    p.add_argument("out")
    p.add_argument("dump", nargs="+")
    p.add_argument("--per", type=int, default=1500)
    p = sp.add_parser("score")
    p.add_argument("npz")
    a = ap.parse_args()
    {"real": cmd_real, "score": cmd_score}[a.cmd](a)


if __name__ == "__main__":
    main()
