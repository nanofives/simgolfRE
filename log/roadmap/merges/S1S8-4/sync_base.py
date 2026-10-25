"""I4: before apply_shared.py runs on the mirror, give the mirror the shared tree's bytes of every shared file the merge edits that
neither section list holds, after checking they equal HEAD's text up to line endings (git archive wrote CRLF where the shared working
tree holds LF; without this step the final byte-identity check would fail on line endings alone). The six files of S1's diff are
checked the other way after apply_shared.py: the mirror (HEAD + diff) must equal the shared tree (where the diff already is).
Usage: ~/bin/py -3.12 log/roadmap/merges/S1S8-4/sync_base.py [--check-six]"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
M = ROOT / "port" / "build" / "I4_S1S8"
BASE = ["port/test/test_boot.py", "port/test/test_shots.py", "re/analysis/data/model_defects.md", "port/test/integrator.py",
        "port/src/sim/modules.inc", "port/src/sim/tick_boot.inc", "re/tools/trace_tool.py", "UNCERTAINTIES.md", "CLAUDE.md",
        "port/src/registry.cpp", "re/port/divergences.tsv"]
SIX = ["port/src/sim/tick.cpp", "port/include/portsim/sim/commands.h", "port/src/sim/commands.cpp",
       "port/src/sim/commands_internal.h", "port/src/sim/menu.cpp", "port/include/portsim/sim/chareditor.h"]


def norm(b):
    return b.replace(b"\r\n", b"\n")


bad = 0
for f in (SIX if "--check-six" in sys.argv else BASE):
    a, b = (ROOT / f).read_bytes(), (M / f).read_bytes()
    if a == b:
        print("SAME", f)
    elif norm(a) == norm(b):
        if "--check-six" in sys.argv:
            print("SAME_TEXT (line endings differ)", f)
        else:
            (M / f).write_bytes(a)
            print("SYNCED (line endings only)", f)
    else:
        print("DIFF", f)
        bad += 1
sys.exit(1 if bad else 0)
