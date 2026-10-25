"""S1.5: for every WM_KEYDOWN / WM_CHAR MSG record of the corpus traces, which root HANDLER ids follow it before the next MSG
record (counts by (msg, handler ids)); 'none' = no root handler at all (a key a focus child took)."""
import collections, glob, pathlib, sys
ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as TF
cnt = collections.Counter()
ex = {}
paths = sys.argv[1:] or sorted(glob.glob(str(ROOT / "port/corpus/traces/*.trace")))
for p in paths:
    tr = TF.read(p)
    pend = None
    def close():
        if pend is not None:
            key = (hex(pend[0]), tuple(pend[2]) or "none")
            cnt[key] += 1
            ex.setdefault(key, (pathlib.Path(p).stem, hex(pend[1])))
    for r in tr.records:
        if r.type == TF.MSG:
            close()
            m = r.f["msg"]
            pend = [m, r.f["wp"], []] if m in (0x100, 0x102) else None
        elif r.type == TF.HANDLER and pend is not None:
            pend[2].append((r.f["handler"], r.f["a0"] if r.f["handler"] == 7 else None))
    close()
for k, v in sorted(cnt.items(), key=lambda kv: -kv[1])[:30]:
    print(v, k, ex[k])
