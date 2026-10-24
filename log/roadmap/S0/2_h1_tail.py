"""S0.2: for every checkpoint classified H1WAIT, the number of timeGetTime reads between the frame's H1 wait read and the checkpoint,
and whether a FRAME record follows; used to bound the H1 wait (2_notes.md)."""
import bisect, json, pathlib, sys
sys.path.insert(0, "."); sys.path.insert(0, "re/tools")
import trace_format as TF
from port.test import framegate as FG, boot as B, gfx as GX
out = []
for s in FG.all_scenarios():
    p = B.TRACES / f"{s}.trace"
    if not p.exists():
        continue
    tr = TF.read(p)
    summ = json.loads((GX.REFS / s / "det_b" / "summary.json").read_text())
    calls = [c for c in TF.clock_calls(tr) if c[3] == 1]
    frames = [r.f["pos"] for r in tr.records if r.type == TF.FRAME]
    for c, d in summ["checkpoints"].items():
        o = calls[d["status"]["tgt"]][0]
        g = bisect.bisect_right(frames, o)
        if g == 0:
            continue
        lo = frames[g - 1]
        last_h1, n = None, 0
        for (oo, ra, _s, _k, _v) in calls:
            if oo < lo:
                continue
            if oo > o:
                break
            if FG.H1_WAIT[0] <= ra <= FG.H1_WAIT[1]:
                last_h1, n = oo, 0
            elif last_h1 is not None:
                n += 1
        out.append((s, c, g, n if last_h1 is not None else -1, g < len(frames)))
        print(*out[-1], flush=True)
