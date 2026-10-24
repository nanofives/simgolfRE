"""S0.2: classify() / screen_kind() / terrain_rule() over every checkpoint of scenario_refs.json from the traces alone (no boot);
screen_kind's location test assumes courseActionMenu.setup was drawn. Output: log/roadmap/S0/2_context_census.tsv + counts."""
import json, pathlib, sys
from collections import Counter
sys.path.insert(0, "."); sys.path.insert(0, "re/tools")
import trace_format as TF
from port.test import framegate as FG, boot as B, gfx as GX
rows, cnt = [], Counter()
for s in FG.all_scenarios():
    p = B.TRACES / f"{s}.trace"
    chks = FG.scenario_checkpoints(s)
    if not p.exists():
        for c in chks:
            rows.append((s, c, "-", "NO_TRACE", "-", "-", "-"))
            cnt[("NO_TRACE", "-", "-")] += 1
        continue
    tr = TF.read(p)
    summ = json.loads((GX.REFS / s / "det_b" / "summary.json").read_text())
    calls = [c for c in TF.clock_calls(tr) if c[3] == 1]
    for c in chks:
        o = calls[summ["checkpoints"][c]["status"]["tgt"]][0]
        m = FG.classify(tr, o)
        k = FG.screen_kind(m["context"], m["frame"], m["wait_ticks"], {"courseActionMenu.setup": 1})
        t = FG.terrain_rule(m["context"], m["frame"])[0]
        rows.append((s, c, m["frame"], m["context"], m["wait_ticks"], k, t))
        cnt[(m["context"], k, t)] += 1
pathlib.Path("log/roadmap/S0/2_context_census.tsv").write_text(
    "scenario\tcheckpoint\tframe\tcontext\twait_ticks\tkind\tterrain\n" + "\n".join("\t".join(map(str, r)) for r in rows) + "\n")
print(len(rows))
for k, v in cnt.most_common():
    print(v, *k)
