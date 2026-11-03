"""S11.5: S11.3's endstate step re-run with the patched endstate (log/roadmap/S11/5_endstate.py) over the same open C2 rows.
Writes log/roadmap/S11/5_cache/endstate.json and prints every row whose verdict (or dead_ok) differs from 3_cache/endstate.json.

    ~/bin/py -3.12 log/roadmap/S11/5_rerun.py
"""
import importlib.util, json, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
S11 = ROOT / "log" / "roadmap" / "S11"
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
spec = importlib.util.spec_from_file_location("endstate", S11 / "5_endstate.py")
es = importlib.util.module_from_spec(spec)
sys.modules["endstate"] = es          # 3_triage.py does `import endstate`
spec.loader.exec_module(es)
spec3 = importlib.util.spec_from_file_location("triage3", S11 / "3_triage.py")
t3 = importlib.util.module_from_spec(spec3)
spec3.loader.exec_module(t3)

old = json.loads((S11 / "3_cache" / "endstate.json").read_text())
new, diffs = {}, []
for r in t3.open_rows():
    mod, a = r.get("module") or t3.EXE, t3.n8(r["addr"])
    callers = (r.get("callers") or "none").strip() or "none"
    key = f"{mod}|{a}"
    new[key] = t3.endstate_row(mod, a, callers)
    o = old.get(key)
    if o is None or o["verdict"] != new[key]["verdict"] or o["dead_ok"] != new[key]["dead_ok"]:
        diffs.append((key, o and (o["verdict"], o["dead_ok"]), (new[key]["verdict"], new[key]["dead_ok"]), new[key]["dead_fail"]))
(S11 / "5_cache").mkdir(exist_ok=True)
(S11 / "5_cache" / "endstate.json").write_text(json.dumps(new, sort_keys=True, indent=0))
print(f"rows {len(new)} (3_cache has {len(old)}); verdict/dead_ok changes: {len(diffs)}")
for d in diffs:
    print(d)
