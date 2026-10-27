"""S7.1 survey: what the 61 boot-gate traces hold that a live host would have to answer (log/roadmap/S7/1_notes.md).

    ~/bin/py -3.12 log/roadmap/S7/1_survey.py > log/roadmap/S7/1_survey.tsv
"""
import collections
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as T  # noqa: E402
from port.test import boot  # noqa: E402

cols = ["scenario", "frames", "w32", "vkey", "post", "timer", "cursor_deliv", "final_poll", "end_sub", "handlers",
        "keystate_async", "cursor_rec", "qpc_runs"]
print("\t".join(cols))
for s in boot.scenarios():
    tr = T.read(boot.TRACES / f"{s}.trace")
    c = collections.Counter()
    end_sub = None
    final = 0
    for r in tr.records:
        if r.type == T.DELIVER:
            c[T.DELIVER_KINDS.get(r.sub, "?")] += 1
        elif r.type == T.FRAME:
            c["frames"] += 1
        elif r.type == T.POLL and r.sub & 2:
            final += 1
        elif r.type == T.END:
            end_sub = r.sub
        elif r.type == T.HANDLER:
            c["handlers"] += 1
        elif r.type == T.KEYSTATE and r.sub == 2:
            c["keystate_async"] += 1
        elif r.type == T.CURSOR:
            c["cursor_rec"] += 1
        elif r.type == T.CLOCKRUN and r.sub == 2:
            c["qpc_runs"] += 1
    print("\t".join(str(x) for x in [s, c["frames"], c["w32"], c["vkey"], c["post"], c["timer"], c["cursor"], final, end_sub,
                                     c["handlers"], c["keystate_async"], c["cursor_rec"], c["qpc_runs"]]))
