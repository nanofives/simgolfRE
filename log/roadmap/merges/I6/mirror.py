"""I6 (S6 wave 2 + S3 waves 2 / 3 + S2 wave 5 merge): the mirror port/build/I6 = git archive of HEAD + the junctions of
log/roadmap/S0/export.md plus shim/build, re/replay and extracted (as I4 / I5) + every non-log path of the four files lists.

Sources of the section files:
- S6 wave 2 (log/roadmap/S6/files.txt), S3 wave 2 (S3/files_w2.txt), S3 wave 3 (S3/files_w3.txt): the shared tree (the leads' final
  versions; golfers / terrain_gen equal S6_w2_mirror, flic files equal S3_4b_src, terrain files equal S3_2_root except the two
  recorders the S3.2 worker extended later).
- S2 wave 5 NEW files (13_merge.md section 1): the S2.13 mirror port/build/S2_13_head (they are not in the shared tree).
  Shared files 13_shared.patch edits are produced by the patch, never copied. instances/ entries and directories are skipped.

Usage: ~/bin/py -3.12 log/roadmap/merges/I6/mirror.py [--recopy] [--drop <wave>...]   (waves: s6w2 s3w2 s3w3 s2w5)"""
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
DST = ROOT / "port" / "build" / "I6"
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build", "re/replay", "extracted"]
RM = ROOT / "log" / "roadmap"
S2_NEW = ["port/src/sim/gfx_boot_screens.cpp", "port/test/test_gfx_boot_screens.py", "port/src/sim/gfx_reports.cpp",
          "port/src/sim/gfx_reports.h", "port/test/test_gfx_reports.py", "port/src/sim/gfx_dialogs.cpp", "port/test/test_gfx_dialogs.py"]
WAVES = {"s6w2": RM / "S6" / "files.txt", "s3w2": RM / "S3" / "files_w2.txt", "s3w3": RM / "S3" / "files_w3.txt",
         "s2w5": RM / "S2" / "files_wave5.txt"}


def read_list(p):
    out = []
    for l in p.read_text().splitlines():
        l = l.strip()
        if l and not l.startswith("log/") and not l.startswith("instances/") and not l.endswith("/") and "_img" not in l and ".tsv.d" not in l:
            out.append(l)
    return out


def patched():
    p = (RM / "S2" / "13_shared.patch").read_text(encoding="utf-8")
    return {l[len("+++ b/"):].strip() for l in p.split("\n") if l.startswith("+++ b/")}


def plan(drop):
    items, pt = [], patched()
    for w, lst in WAVES.items():
        if w in drop:
            continue
        for rel in read_list(lst):
            if w == "s2w5":
                if rel in pt:
                    continue
                src = ROOT / "port" / "build" / "S2_13_head" / rel if rel in S2_NEW else ROOT / rel
            else:
                src = ROOT / rel
            items.append((rel, src))
    return items


def main():
    a = sys.argv[1:]
    global DST
    if "--dst" in a:   # another mirror dir under port/build (I6b = the mirror without the refused wave)
        DST = ROOT / "port" / "build" / a[a.index("--dst") + 1]
    recopy = "--recopy" in a
    drop = {w for w in a[a.index("--drop") + 1:] if w in WAVES} if "--drop" in a else set()
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
                assert DST in link.resolve().parents, link
                shutil.rmtree(link)
            if not link.exists():
                if not (ROOT / j).exists():
                    print("no shared dir, skipped junction", j)
                    continue
                link.parent.mkdir(parents=True, exist_ok=True)
                subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(ROOT / j)], check=True, capture_output=True)
                print("junction", j)
    for rel, src in plan(drop):
        assert src.is_file(), (rel, src)
        d = DST / rel
        d.parent.mkdir(parents=True, exist_ok=True)
        d.write_bytes(src.read_bytes())
        print("copied", rel, "<-", src.relative_to(ROOT))


if __name__ == "__main__":
    main()
