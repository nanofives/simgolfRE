"""I7 (S2 waves 6 / 7 + S5 wave 1 + S7 waves 1 / 2 merge): the mirror port/build/I7 = git archive of HEAD + the junctions of
log/roadmap/S0/export.md plus shim/build, re/replay and extracted (as I4..I6) + the new / section files of each wave.

Sources (checked byte-equal before the merge, log/roadmap/merges/I7/sources.log):
- S2 wave 6 (S2.10, 10_merge.md section 1): the six new files from port/build/S2_10_head (not in the shared tree).
- S2 wave 7 (S2.11, 11_merge.md section 1): the four new files from port/build/S2_11w6_head (not in the shared tree).
  Every other file of the two waves is produced by 10_shared.patch / 11_shared.patch (apply_shared.py), never copied.
- S5 wave 1 (S5/files.txt): its non-log paths from the shared tree (equal to port/build/S5_4_src where that has them).
- S7 wave 1 (S7/MERGE.md sections 1-3): live.h / live.cpp / live.py / test_live.py from port/build/S7_7_head (not in the shared
  tree); port/app/** and the app tests from the shared tree (S7.4's two patches already applied there by the lead).
- S7 wave 2 (S7.5, 5_merge.md section 1): port/app/main.cpp, host.h, host.cpp and port/test/test_app_ingame.py from
  port/build/S7_5_head (over wave 1's copies). test_app_live.py gets 5_test_app_live.patch (apply_shared.py).

Usage: ~/bin/py -3.12 log/roadmap/merges/I7/mirror.py [--dst <dir under port/build>] [--recopy] [--drop <wave>...]
waves: s2w6 s2w7 s5 s7w1 s7w2"""
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
B = ROOT / "port" / "build"
DST = B / "I7"
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build", "re/replay", "extracted"]
RM = ROOT / "log" / "roadmap"

S2W6 = ["port/src/sim/buyland.cpp", "port/src/sim/gfx_buyland.cpp", "port/src/sim/gfx_chareditor.cpp", "port/test/test_buyland.py",
        "port/test/golden_buyland.json", "port/test/test_gfx_s2_10.py"]
S2W7 = ["port/src/sim/gfx_results.cpp", "port/src/sim/gfx_simfoto.cpp", "port/src/sim/gfx_bulletin.cpp", "port/test/test_gfx_dialogs2.py"]
S7_LIVE = ["port/include/portsim/sim/live.h", "port/src/sim/live.cpp", "port/test/live.py", "port/test/test_live.py"]
S7_TESTS = ["port/test/test_app_input.py", "port/test/test_app_shell.py", "port/test/test_app_live.py", "port/test/app_replay.py",
            "port/test/test_app_replay.py", "re/roadmap/S7_steps.tsv"]
S7W2 = ["port/app/main.cpp", "port/app/host.h", "port/app/host.cpp", "port/test/test_app_ingame.py"]


def s5_files():
    out = []
    for l in (RM / "S5" / "files.txt").read_text().splitlines():
        l = l.strip()
        if l and not l.startswith("log/") and not l.endswith("/") and l != "port/test/gfx.py":
            out.append(l)
    return out


def app_files():
    out = []
    for p in sorted((ROOT / "port" / "app").rglob("*")):
        if p.is_file() and "__pycache__" not in p.parts:
            out.append(p.relative_to(ROOT).as_posix())
    return out


def plan(drop):
    items = []
    if "s2w6" not in drop:
        items += [(r, B / "S2_10_head" / r) for r in S2W6]
    if "s2w7" not in drop:
        items += [(r, B / "S2_11w6_head" / r) for r in S2W7]
    if "s5" not in drop:
        items += [(r, ROOT / r) for r in s5_files()]
    if "s7w1" not in drop:
        items += [(r, B / "S7_7_head" / r) for r in S7_LIVE]
        items += [(r, ROOT / r) for r in app_files() + S7_TESTS]
        if "s7w2" not in drop:
            items += [(r, B / "S7_5_head" / r) for r in S7W2]
    return items


def main():
    a = sys.argv[1:]
    global DST
    if "--dst" in a:
        DST = B / a[a.index("--dst") + 1]
    recopy = "--recopy" in a
    drop = set(a[a.index("--drop") + 1:]) if "--drop" in a else set()
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
        print("copied", rel, "<-", src.relative_to(ROOT).as_posix())
    print("dropped:", sorted(drop) or "none")


if __name__ == "__main__":
    main()
