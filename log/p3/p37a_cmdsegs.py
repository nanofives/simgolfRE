"""Phase 3.7a: run the accepted COMMAND segments through sg_run_frames with the trace-driven input (tick.cpp TraceInput).

One row per segment: status, frames run / expected, the first divergent record with its objects (and the first differing
byte of the first object), the non-causal verdict (port/test/segments.py NON_CAUSAL), and sg_run_frames_detail (why it
stopped, the input summary). Output: log/p3/p37a_cmdsegs.tsv (or --out).
usage: ~/bin/py -3.12 log/p3/p37a_cmdsegs.py [--jobs 8] [--only <scenario/kk>,...] [--class COMMAND|TIME-FORWARD|all]
SG_PORTSIM_DLL selects the DLL.
"""
import argparse
import concurrent.futures as CF
import ctypes as C
import json
import pathlib
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))


def run_one(name):
    from port.test import lib as L
    from port.test import segments as S
    lib = L.load()
    lib.dll.sg_run_frames_detail.restype = C.c_char_p
    lib.dll.sg_run_frames_detail.argtypes = [C.c_void_p]
    scen, k = name.split("/")
    seg = S.load_segment(S.SEG_DIR / scen / k)
    runner = S.SegmentRunner(lib)
    t0 = time.time()
    res = runner.run(seg)
    row = {"segment": name, "status": res.status, "frames": f"{res.frames_run}/{res.frames_expected}",
           "seconds": round(time.time() - t0, 1), "record": "", "objects": "", "first_byte": "",
           "noncausal": getattr(res, "noncausal", ""), "detail": res.detail.replace("\t", " ")}
    div = res.first_divergence
    if div:
        row["record"] = str(div.get("record"))
        row["objects"] = ",".join(div.get("objects", []))[:300]
    row["run_detail"] = getattr(res, "run_detail", "")
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--only", default="")
    ap.add_argument("--cls", default="COMMAND")
    ap.add_argument("--out", default=str(ROOT / "log" / "p3" / "p37a_cmdsegs.tsv"))
    a = ap.parse_args()
    from port.test import segments as S
    rows = [r for r in S.manifest_rows() if r["status"] == "accepted" and (a.cls == "all" or r["class"] == a.cls)]
    names = [f"{r['scenario']}/{int(r['segment']):02d}" for r in rows]
    if a.only:
        want = set(a.only.split(","))
        names = [n for n in names if n in want]
    out = []
    with CF.ProcessPoolExecutor(a.jobs) as ex:
        futs = {ex.submit(run_one, n): n for n in names}
        done = 0
        for f in CF.as_completed(futs):
            done += 1
            try:
                r = f.result()
            except Exception as e:  # noqa: BLE001
                r = {"segment": futs[f], "status": "ERROR", "detail": repr(e)[:300]}
            out.append(r)
            print(f"[{done}/{len(names)}] {r['segment']} {r['status']} {r.get('frames', '')} rec={r.get('record', '')} "
                  f"{r.get('objects', '')[:80]} | {r.get('run_detail', '')[:160]}", flush=True)
    out.sort(key=lambda r: r["segment"])
    cols = ["segment", "status", "frames", "record", "objects", "noncausal", "seconds", "run_detail", "detail"]
    with open(a.out, "w", encoding="utf-8") as fh:
        fh.write("\t".join(cols) + "\n")
        for r in out:
            fh.write("\t".join(str(r.get(c, "")) for c in cols) + "\n")
    n = {}
    for r in out:
        n[r["status"]] = n.get(r["status"], 0) + 1
    print(json.dumps(n))


if __name__ == "__main__":
    main()
