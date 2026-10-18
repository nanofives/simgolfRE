r"""Compare two traced scenario runs: per-object checkpoint hashes and per-frame hashes (phase 2.2, 2026-10-18).

Inputs are run folders written by re/frida/scenario_ab.py with the opt-in tracer:
  <name>.objhash.json   C3FT_OBJHASH: {symbol: [class, sgh64]} for every GameState object at checkpoint <name>;
  framehash.bin / .json C3FT_FRAMEHASH: one record per mainLoop frame head (the timeGetTime call at 0x0040fc29):
                        u32 T (the virtual time handed to the game), u32 frame-head index, u64 sgh64 per object of the
                        traced classes (framehash.json lists the symbols and the fetches).
The FIRST DIVERGENT FRAME is the first record index where the two runs differ in T, frame index or any object hash;
the report names the objects that differ there, so a divergence is located to a frame and objects without re-running.

    ~/bin/py -3.12 re/tools/frame_trace.py cmp <run dir a> <run dir b> [--classes sim] [--json out.json]
"""
from __future__ import annotations

import argparse
import json
import pathlib
import sys

import numpy as np


def read_frames(run: pathlib.Path):
    """(meta, T[u32], index[u32], hashes[uint64, records x objects]) of a run folder; None without framehash.json."""
    run = pathlib.Path(run)
    mp = run / "framehash.json"
    if not mp.exists():
        return None
    meta = json.loads(mp.read_text())
    n = len(meta["symbols"])
    rec = 8 + 8 * n
    blob = (run / "framehash.bin").read_bytes() if (run / "framehash.bin").exists() else b""
    if len(blob) % rec:
        raise ValueError(f"{run}: framehash.bin is {len(blob)} bytes, not a multiple of the {rec}-byte record")
    k = len(blob) // rec
    dt = np.dtype([("T", "<u4"), ("idx", "<u4"), ("h", "<u8", (n,))])
    arr = np.frombuffer(blob, dt, count=k)
    return meta, arr["T"], arr["idx"], arr["h"].reshape(k, n)


def compare_frames(a: pathlib.Path, b: pathlib.Path) -> dict:
    fa, fb = read_frames(a), read_frames(b)
    if fa is None or fb is None:
        return {"error": "a run has no framehash.json"}
    ma, Ta, Ia, Ha = fa
    mb, Tb, Ib, Hb = fb
    if ma["symbols"] != mb["symbols"]:
        return {"error": "the runs traced different object lists"}
    dropped = [sum(f.get("dropped", 0) for f in m["fetches"]) for m in (ma, mb)]
    n = min(len(Ta), len(Tb))
    out = {"records": [int(len(Ta)), int(len(Tb))], "compared": int(n), "dropped": dropped, "objects": len(ma["symbols"]),
           "first_divergent": None}
    if n:
        bad = (Ta[:n] != Tb[:n]) | (Ia[:n] != Ib[:n]) | (Ha[:n] != Hb[:n]).any(axis=1)
        hit = np.nonzero(bad)[0]
        if len(hit):
            i = int(hit[0])
            objs = [ma["symbols"][j] for j in np.nonzero(Ha[i] != Hb[i])[0]]
            out["first_divergent"] = {"record": i, "T": [int(Ta[i]), int(Tb[i])], "frame": [int(Ia[i]), int(Ib[i])],
                                      "objects": objs, "divergent_records": int(len(hit)),
                                      "previous_T": int(Ta[i - 1]) if i else None}
    out["same"] = out["first_divergent"] is None and len(Ta) == len(Tb) and dropped == [0, 0]
    return out


def objhash_names(run: pathlib.Path) -> list[str]:
    return sorted(p.name[:-len(".objhash.json")] for p in pathlib.Path(run).glob("*.objhash.json"))


def compare_objhash(a: pathlib.Path, b: pathlib.Path, classes=None) -> dict:
    out = {"checkpoints": {}, "missing": []}
    names = sorted(set(objhash_names(a)) | set(objhash_names(b)))
    for name in names:
        pa, pb = pathlib.Path(a) / f"{name}.objhash.json", pathlib.Path(b) / f"{name}.objhash.json"
        if not (pa.exists() and pb.exists()):
            out["missing"].append(name)
            continue
        ha, hb = json.loads(pa.read_text())["hashes"], json.loads(pb.read_text())["hashes"]
        diff = {}
        for k in sorted(set(ha) | set(hb)):
            va, vb = ha.get(k), hb.get(k)
            cls = (va or vb)[0]
            if classes and cls not in classes:
                continue
            if va != vb:
                diff.setdefault(cls, []).append(k)
        out["checkpoints"][name] = diff
    out["first_divergent"] = next((n for n in names if out["checkpoints"].get(n)), None)
    out["same"] = not out["missing"] and out["first_divergent"] is None
    return out


def print_report(fr: dict, oh: dict):
    print("frame-head hashes:", end=" ")
    if "error" in fr:
        print(fr["error"])
    else:
        print(f"records {fr['records']}, compared {fr['compared']}, objects {fr['objects']}, dropped {fr['dropped']}")
        d = fr["first_divergent"]
        if d:
            print(f"  FIRST DIVERGENT FRAME: record {d['record']} T {d['T']} frame index {d['frame']} (previous record "
                  f"T {d['previous_T']}), {d['divergent_records']} divergent records; objects: {', '.join(d['objects'])}")
        print("  SAME" if fr["same"] else "  DIFF")
    print(f"checkpoint object hashes: {len(oh['checkpoints'])} checkpoints, missing {oh['missing']}")
    for name, diff in oh["checkpoints"].items():
        if diff:
            print(f"  {name}: " + "; ".join(f"{c} {len(v)}: {', '.join(v[:8])}{' ...' if len(v) > 8 else ''}"
                                          for c, v in sorted(diff.items())))
    print("  SAME" if oh["same"] else f"  DIFF (first at {oh['first_divergent']})")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("cmp")
    p.add_argument("a")
    p.add_argument("b")
    p.add_argument("--classes", help="checkpoint hash classes to compare (default all)")
    p.add_argument("--json")
    a = ap.parse_args(argv)
    fr = compare_frames(a.a, a.b)
    oh = compare_objhash(a.a, a.b, set(a.classes.split(",")) if a.classes else None)
    print_report(fr, oh)
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps({"frames": fr, "objhash": oh}, indent=1))
    return 0 if fr.get("same") and oh["same"] else 1


if __name__ == "__main__":
    sys.exit(main())
