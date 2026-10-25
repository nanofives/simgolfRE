"""I5: every non-log path of the five files lists (S2 files.txt, files_wave2/3/4.txt, S3 files.txt) and every shared file the merge edited,
shared tree against the mirror port/build/I5_S2S3: `same` (byte-identical), `EOL-only` (equal after CRLF -> LF), `DIFF`.
Usage: ~/bin/py -3.12 log/roadmap/merges/S2S3-5/identity_check.py"""
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[4]
M = ROOT / "port" / "build" / "I5_S2S3"
RM = ROOT / "log" / "roadmap"
LISTS = [RM / "S2" / "files.txt", RM / "S2" / "files_wave2.txt", RM / "S2" / "files_wave3.txt", RM / "S2" / "files_wave4.txt",
         RM / "S3" / "files.txt"]
EDITED = ["port/test/test_static.py", "port/test/README.md", "UNCERTAINTIES.md", "CLAUDE.md", "re/tools/scenario.py",
          "re/analysis/data/scenario_refs.json", "port/src/portsim_c.cpp", "port/include/portsim_c.h", "port/src/sim/tick.cpp",
          "port/src/sim/commands.cpp", "port/src/sim/commands_screens.inc", "port/src/sim/routing.cpp", "port/src/sim/saveload.cpp",
          "port/src/sim/tick_boot.inc", "port/src/sim/gfx.cpp", "port/src/sim/gfx_body.h", "port/src/sim/gfx_families.inc",
          "re/PHASE5_TERRAIN_PLAN.md", "port/test/test_s8_2_scenarios.py", "re/tools/scenario_s8_2.py"]
paths = []
for lst in LISTS:
    for l in lst.read_text().splitlines():
        l = l.strip()
        if l and not l.startswith("log/") and not l.endswith("/") and l not in paths:
            paths.append(l)
for e in EDITED:
    if e not in paths:
        paths.append(e)
n = {"same": 0, "EOL-only": 0, "DIFF": 0}
for rel in paths:
    a, b = (ROOT / rel).read_bytes(), (M / rel).read_bytes()
    st = "same" if a == b else ("EOL-only" if a.replace(b"\r\n", b"\n") == b.replace(b"\r\n", b"\n") else "DIFF")
    n[st] += 1
    print(st, rel)
print(len(paths), "paths", n)
