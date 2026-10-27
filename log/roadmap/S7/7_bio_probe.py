"""S7.7: the record sequence of cov_r_bio's corpus trace around the keys the bio EditBox took (MSG 0x100 / 0x102 with no HANDLER
after it, parse_trace's RootHandler 12 / 13), so the live script session `bio_type` posts its keys the way the original saw them.

    ~/bin/py -3.12 log/roadmap/S7/7_bio_probe.py [scenario] [--n 60]     (run from a mirror root or the shared root)
"""
import pathlib
import sys

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as TF  # noqa: E402

scen = next((a for a in sys.argv[1:] if not a.startswith("--")), "cov_r_bio")
n_show = int(sys.argv[sys.argv.index("--n") + 1]) if "--n" in sys.argv else 60
tr = TF.parse((ROOT / "port" / "corpus" / "traces" / f"{scen}.trace").read_bytes())
recs = tr.records
focus = []
for i, r in enumerate(recs):
    if r.type == TF.MSG and r.f["msg"] in (0x100, 0x102):
        j = i + 1
        while j < len(recs) and recs[j].type not in (TF.MSG, TF.HANDLER, TF.FRAME, TF.CLOCKRUN):
            j += 1
        if j >= len(recs) or recs[j].type != TF.HANDLER:
            focus.append(i)
print(scen, "records", len(recs), "focus keys", len(focus))
if focus:
    lo = max(0, focus[0] - 12)
    for r in recs[lo:lo + n_show]:
        name = TF.TYPE_NAMES.get(r.type, r.type)
        f = {k: (hex(v) if isinstance(v, int) and k in ("msg", "ra", "addr") else v) for k, v in r.f.items() if k != "snapshot"}
        print(name, r.sub, f)
