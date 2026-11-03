"""P5A items 3 and 4 in the app: sessions driven by posted messages (port/test/app_replay.py's Driver), recorded into a
scratch directory (never port/corpus/app), each replayed through sg_run_boot against its own framehash.bin (gate_one).
  worldmap   title -> Sandbox -> one WM_MOUSEMOVE onto Monterey's map pin (270, 32), then 2 s with no input, then Esc...
             counts the presents after the move (presents.tsv) and keeps the last one's position
  report     app_replay's `report` script (F1..F4 report screens)
Run from a tree root:  ~/bin/py -3.12 <this> <scratch dir> <exe> [script ...] [-- extra app args]"""
import json, pathlib, sys, time
ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
from port.test import app_replay as A
from port.test import lib as L

scratch = pathlib.Path(sys.argv[1]); scratch.mkdir(parents=True, exist_ok=True)
exe = pathlib.Path(sys.argv[2])
rest = sys.argv[3:]
extra = rest[rest.index("--") + 1:] if "--" in rest else []
scripts = rest[:rest.index("--")] if "--" in rest else rest
A.CORPUS = scratch


def script_worldmap(d):
    A.wait_shown_hash(d.rec, A.title_menu_hash(), 120)
    d.wait_quiet(1.5, 120)
    d.move(130, 445)
    d.wait_quiet(1.0, 20)
    d.click(130, 445, tag="Sandbox")
    time.sleep(3.0)                                # the location screen is up (its loads, the first passes)
    (d.rec / "p5a_mark_before_move").write_text(str(time.time()))
    d.move(270, 32)           # the only input after this point until the Esc
    time.sleep(2.0)
    (d.rec / "p5a_mark_after_wait").write_text(str(time.time()))
    d.click(768, 557, tag="back")                  # leave via the back arrow, then quit the app
    time.sleep(1.5)


A.SCRIPTS["worldmap"] = script_worldmap
lib = L.load()
for s in scripts:
    name = f"p5a_{s}" + ("_off" if "--no-timed-passes" in extra else "")
    out = A.record(s, name, exe, extra=tuple(extra))
    pres = (out / "presents.tsv").read_text().splitlines()
    res = A.gate_one(lib, name)
    print(name, "presents", len(pres) - 1, "gate", json.dumps(res, default=str)[:400])
