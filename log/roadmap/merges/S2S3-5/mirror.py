"""I5 (S2 waves 1-4 + S3 wave 1 merge): the mirror port/build/I5_S2S3 = git archive of HEAD + the junctions of
log/roadmap/S0/export.md plus shim/build, re/replay and extracted (as I4) + every non-log path of the section files lists.

Sources of the section files:
- S2 wave 1 (files.txt), wave 4 (files_wave4.txt), S3 wave 1 (S3/files.txt): the shared tree (checked equal to the leads'
  mirrors port/build/S2_1_head and port/build/S3_head, except test_gfx_eoi.py whose shared copy carries the S3 lead's U-1120
  citation fix).
- S2 wave 2 / wave 3 NEW files: the leads' mirrors port/build/S2_3_head / port/build/S2_4_head (MERGE_wave2.md / MERGE_wave3.md
  section 1: nothing of them is in the shared tree). Shared files those lists name are produced by the patches, not copied.
- re/analysis/data/scenario_refs.json: the shared copy (= HEAD + the key cov_r_s2_screens + its `updated` stamp, checked).

Refuses to run over an existing mirror unless --recopy (then only the section files are copied again).
Usage: ~/bin/py -3.12 log/roadmap/merges/S2S3-5/mirror.py [--recopy]"""
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
DST = ROOT / "port" / "build" / "I5_S2S3"
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build", "re/replay", "extracted"]
RM = ROOT / "log" / "roadmap"
# files the shared patches create or edit: never copied from a list (the patches make them)
PATCHED = {"port/src/sim/gfx_body.h", "port/src/sim/gfx.cpp", "port/src/sim/gfx_families.inc", "port/src/sim/commands.cpp",
           "port/src/sim/commands_screens.inc", "port/src/sim/tick.cpp", "port/src/sim/routing.cpp", "port/src/sim/saveload.cpp",
           "port/src/sim/tick_boot.inc"}
WAVE1_SHARED = {"port/src/sim/gfx.cpp", "port/src/sim/gfx_body.h", "port/src/sim/gfx_families.inc"}  # wave 1 base of the patches


def read_list(p):
    out = []
    for l in p.read_text().splitlines():
        l = l.strip()
        if l and not l.startswith("log/") and not l.endswith("/"):
            out.append(l)
    return out


def plan():
    """(rel, source path) in copy order."""
    items = []
    for rel in read_list(RM / "S2" / "files.txt"):          # wave 1, shared tree (incl. the wave-1 gfx files the patches start from)
        items.append((rel, ROOT / rel))
    for lst, mirror in (("files_wave2.txt", "S2_3_head"), ("files_wave3.txt", "S2_4_head")):
        for rel in read_list(RM / "S2" / lst):
            if rel in PATCHED:
                continue
            src = ROOT / rel if rel.startswith("re/roadmap/") else ROOT / "port" / "build" / mirror / rel
            items.append((rel, src))
    for rel in read_list(RM / "S2" / "files_wave4.txt"):
        items.append((rel, ROOT / rel))
    for rel in read_list(RM / "S3" / "files.txt"):
        items.append((rel, ROOT / rel))
    items.append(("re/analysis/data/scenario_refs.json", ROOT / "re/analysis/data/scenario_refs.json"))
    return items


def main():
    recopy = "--recopy" in sys.argv[1:]
    if (DST / "port" / "CMakeLists.txt").exists() and not recopy:
        sys.exit(f"{DST} exists (use --recopy)")
    if not recopy:
        DST.mkdir(parents=True, exist_ok=True)
        head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
        tar = subprocess.run(["git", "archive", head], cwd=ROOT, capture_output=True, check=True).stdout
        subprocess.run(["tar", "-xf", "-"], cwd=DST, input=tar, check=True)
        (DST / "HEAD_COMMIT").write_text(head + "\n")
        for j in JUNCTIONS:
            link = DST / j
            if link.exists() and not os.path.isjunction(link):
                assert DST in link.resolve().parents, link   # the archive's tracked copy, inside DST only
                shutil.rmtree(link)
            if not link.exists():
                if not (ROOT / j).exists():
                    print("no shared dir, skipped junction", j)
                    continue
                link.parent.mkdir(parents=True, exist_ok=True)
                subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(ROOT / j)], check=True, capture_output=True)
                print("junction", j)
    for rel, src in plan():
        assert src.is_file(), (rel, src)
        d = DST / rel
        d.parent.mkdir(parents=True, exist_ok=True)
        d.write_bytes(src.read_bytes())
        print("copied", rel, "<-", src.relative_to(ROOT))


if __name__ == "__main__":
    main()
