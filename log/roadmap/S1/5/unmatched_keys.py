"""Count WM_KEYDOWN / WM_CHAR MSG records of each corpus trace that no root handler (HANDLER 8 / 7) follows before the
next MSG record (S1.5: keys the window tree's focus child consumed)."""
import glob, pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as TF
want = {0x100: 8, 0x102: 7}
tot = 0
paths = sys.argv[1:] or sorted(glob.glob(str(ROOT / "port/corpus/traces/*.trace")))
for p in paths:
    tr = TF.read(p)
    pend, un, n = None, [], 0
    for r in tr.records:
        if r.type == TF.MSG:
            if pend is not None:
                un.append(pend)
            m = r.f["msg"]
            pend = (m, r.f["wp"], r.f["pos"]) if m in want else None
            n += m in want
        elif r.type == TF.HANDLER and pend is not None:
            if r.f["handler"] == want[pend[0]]:
                pend = None
    if pend is not None:
        un.append(pend)
    tot += len(un)
    if un or len(paths) < 5:
        print(pathlib.Path(p).stem, n, "key msgs,", len(un), "unmatched", [(hex(m), hex(w), pos) for m, w, pos in un[:12]])
print("total unmatched", tot, "over", len(paths), "traces")
