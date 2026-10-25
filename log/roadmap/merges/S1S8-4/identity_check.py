"""I4: byte identity between the shared tree and the mirror port/build/I4_S1S8 for every non-log path of S1's and S8's files.txt and
every shared file the merge edited. The log/ paths of files.txt are the same files through the mirror's `log` junction.
Usage: ~/bin/py -3.12 log/roadmap/merges/S1S8-4/identity_check.py"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
M = ROOT / "port" / "build" / "I4_S1S8"
EDITED = ["port/src/sim/tick.cpp", "port/include/portsim/sim/commands.h", "port/src/sim/commands.cpp",
          "port/src/sim/commands_internal.h", "port/src/sim/menu.cpp", "port/include/portsim/sim/chareditor.h",
          "port/test/test_s1_4_scenarios.py", "port/test/test_boot.py", "re/analysis/data/model_defects.md", "port/test/integrator.py",
          "port/test/test_shots.py", "port/src/sim/modules.inc", "port/src/registry.cpp", "port/src/sim/tick_boot.inc",
          "re/tools/scenario.py", "re/tools/trace_tool.py", "UNCERTAINTIES.md", "CLAUDE.md", "port/test/test_s8_2_scenarios.py",
          "re/tools/scenario_s8_2.py", "port/test/test_video.py", "re/port/divergences.tsv"]
paths = []
for lst in ("log/roadmap/S1/files.txt", "log/roadmap/S8/files.txt"):
    for l in (ROOT / lst).read_text().splitlines():
        l = l.strip()
        if l and not l.startswith("log/") and not l.endswith("/") and l not in paths:
            paths.append(l)
paths += [p for p in EDITED if p not in paths]
bad = []
for p in paths:
    same = (ROOT / p).read_bytes() == (M / p).read_bytes()
    print("SAME" if same else "DIFF", p)
    if not same:
        bad.append(p)
print(f"{len(paths)} paths, {len(paths) - len(bad)} identical, differing: {bad}")
sys.exit(1 if bad else 0)
