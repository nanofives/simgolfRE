"""38f: what the original runs between a mainLoop exit to the title menu and the next frame head (the stretch sg_run_frames
does not run). For each case: the frame-head interval, the main-thread timeGetTime calls by the exe function that made them
(return address -> re/functions_ghidra.tsv range; jgld / other modules by their slot tag), and the recorded input there.

    ~/bin/py -3.12 log/p38/38f_menu_gap.py    -> log/p38/38f/menu_gap.json and a printed table
"""
import bisect
import collections
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as TF  # noqa: E402
from port.test import segments as S  # noqa: E402

# (segment, segment-local frame index of the last frame head before the exit): the in-segment exits measured by 38a
# (segment runs stop after frame 18 / 19 / 18 / 18) and the cov_b_shots round boundaries (the last record of the segment).
CASES = [("cov_b_career_types/00", 18), ("cov_b_career_types/01", 19), ("cov_b_career_types/02", 18),
         ("cov_k1_animals/00", 18), ("cov_b_shots/02", None), ("cov_b_shots2/01", None), ("cov_b_shots2/03", None)]


def functions():
    rows = []
    for line in (ROOT / "re" / "functions_ghidra.tsv").read_text().splitlines()[1:]:
        p = line.split("\t")
        for rg in p[5].split(";") if len(p) > 5 and p[5] else []:
            lo, hi = rg.split("-")
            rows.append((int(lo, 16), int(hi, 16), p[1]))
    rows.sort()
    return rows


def owner(fn, ra):
    i = bisect.bisect_right([r[0] for r in fn], ra) - 1
    if i >= 0 and fn[i][0] <= ra < fn[i][1]:
        return fn[i][2]
    return f"0x{ra:08x}"


def gap(name, local, fn):
    scen, k = name.split("/")
    seg = S.load_segment(S.SEG_DIR / scen / k)
    nxt = None
    if local is None:              # a boundary: the gap runs into the next segment's first frame head (whole trace needed)
        local = len(seg.records) - 1
        nxt = S.load_segment(S.SEG_DIR / scen / f"{int(k) + 1:02d}")
    tr = TF.parse(seg.trace) if nxt is None else TF.read(ROOT / "port" / "corpus" / "traces" / f"{scen}.trace")
    frames = [i for i, r in enumerate(tr.records) if r.type == TF.FRAME]
    if nxt is None:
        a = frames[local]
        b = frames[local + 1] if local + 1 < len(frames) else len(tr.records)
    else:
        t_last, t_next = seg.records[-1][0], nxt.records[0][0]
        a = next(i for i in frames if tr.records[i].f["t"] == t_last)
        b = next(i for i in frames if tr.records[i].f["t"] == t_next)
    calls = collections.Counter()
    order = []
    inputs = collections.Counter()
    for r in tr.records[a + 1:b]:
        if r.type == TF.CLOCKRUN and r.f["kind"] == 1:
            ra = r.f["ra"]
            o = owner(fn, ra) if 0x00400000 <= ra < 0x00500000 else f"slot{r.f['slot']}:0x{ra:08x}"
            calls[o] += r.f["count"]
            if not order or order[-1] != o:
                order.append(o)
        elif r.type in (TF.HANDLER, TF.MSG, TF.DELIVER, TF.TAKE):
            inputs[r.name + (f"/{TF.HANDLERS.get(r.sub, r.sub)}" if r.type == TF.HANDLER else "")] += 1
    t0 = tr.records[a].f["t"]
    t1 = tr.records[b].f["t"] if b < len(tr.records) and tr.records[b].type == TF.FRAME else None
    return {"case": name, "frame": local, "T_exit_frame": t0, "T_next_frame": t1,
            "interval_ms": None if t1 is None else t1 - t0, "tgt_calls_by_function": dict(calls.most_common()),
            "function_order": order[:40], "inputs": dict(inputs)}


def main():
    fn = functions()
    out = [gap(n, l, fn) for n, l in CASES]
    (ROOT / "log" / "p38" / "38f").mkdir(parents=True, exist_ok=True)
    (ROOT / "log" / "p38" / "38f" / "menu_gap.json").write_text(json.dumps(out, indent=1))
    for g in out:
        print(f"== {g['case']} frame {g['frame']}: interval {g['interval_ms']} virtual ms; inputs {g['inputs']}")
        for f, n in list(g["tgt_calls_by_function"].items())[:14]:
            print(f"   {n:7d} {f}")
        print("   order:", " > ".join(g["function_order"][:24]))


if __name__ == "__main__":
    main()
