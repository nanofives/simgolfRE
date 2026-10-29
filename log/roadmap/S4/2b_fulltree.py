"""S4.2b: the whole-tree mirror the resubmitted wave is judged in (I3's port/build/I3_S4 pattern, log/roadmap/merges/S4-3.md).

port/build/S4_2b_full = `git archive HEAD` + the junctions of log/roadmap/S0/export.md (original, log, port/corpus, tests/golden,
tests/fixtures, re/versions, tools/vc6) plus shim/build and re/replay, + the wave's files copied from the shared tree (every non-log
path of log/roadmap/S4/{files,1..9b,2b}_files.txt and the three files S4.2b added: test_audio_names.py and the S4.9 / S4.9b
fixtures), + the shared-file edits of log/roadmap/S4/2b_apply_shared.py. Siblings' S4.10 / S4.11 files are not copied.

    ~/bin/py -3.12 log/roadmap/S4/2b_fulltree.py [--fresh]
"""
import argparse
import io
import pathlib
import shutil
import subprocess
import sys
import tarfile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
DST = ROOT / "port" / "build" / "S4_2b_full"
JUNCTIONS = ("original", "log", "port/corpus", "tests/golden", "tests/fixtures", "re/versions", "tools/vc6", "shim/build", "re/replay",
             "extracted")
LISTS = ("files", "1_files", "2_files", "2b_files", "3_files", "4_files", "5_files", "6_files", "7_files", "8_files", "9_files",
         "9b_files", "10_files", "11_files")
EXTRA = ("port/test/test_audio_names.py", "port/test/test_bridge.py", "port/test/test_commands.py", "port/test/test_dialogs.py",
         "port/test/test_chareditor.py", "port/test/test_chareditor_bio.py", "port/test/test_reports.py", "port/test/test_events.py",
         "port/test/test_framegate.py", "port/test/liveraw.py", "port/test/sound_fixtures/s49_wave.js", "port/test/sound_fixtures/s49b_soundfile.js")


def wave_files():
    out = set(EXTRA)
    for n in LISTS:
        for line in (HERE / f"{n}.txt").read_text(encoding="utf-8").splitlines():
            if not line.strip() or line[0].isspace():
                continue                    # blank, or the wrapped comment of the line before
            p = line.split()[0]
            if "/" in p and not p.endswith("/") and not p.startswith(("log/", "#", "port/corpus/")) and "<" not in p and "*" not in p:
                out.add(p)
    return sorted(out)


def _is_junction(p: pathlib.Path) -> bool:
    try:
        return p.is_junction()
    except AttributeError:
        return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fresh", action="store_true")
    a = ap.parse_args()
    if a.fresh and DST.exists():
        for j in JUNCTIONS:                     # remove the links first so rmtree never walks into the shared dirs
            p = DST / j
            if _is_junction(p):
                p.unlink()
        shutil.rmtree(DST)
    if not DST.exists():
        DST.mkdir(parents=True)
        tar = subprocess.run(["git", "archive", "HEAD"], cwd=ROOT, capture_output=True, check=True).stdout
        with tarfile.open(fileobj=io.BytesIO(tar)) as t:
            t.extractall(DST, filter="fully_trusted")
        head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True, check=True).stdout
        (DST / "HEAD_COMMIT").write_text(head)
        for j in JUNCTIONS:
            p = DST / j
            if p.exists():
                shutil.rmtree(p) if p.is_dir() else p.unlink()
            p.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["cmd", "/c", "mklink", "/J", str(p).replace("/", "\\"), str(ROOT / j).replace("/", "\\")],
                           check=True, capture_output=True)
        print("archive + junctions:", DST)
        fresh_shared = True
    else:
        fresh_shared = False
    n = 0
    for f in wave_files():
        if not fresh_shared and f == "re/roadmap/S4_functions.tsv":
            continue                            # 2b_apply_shared.py edited the copy (S4.11's rows): a resync keeps it
        s, d = ROOT / f, DST / f
        if not s.exists():
            sys.exit(f"missing in the tree: {f}")
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or d.read_bytes() != s.read_bytes():
            shutil.copy2(s, d)
            n += 1
    print(f"wave files copied: {n} of {len(wave_files())}")
    if fresh_shared:
        # the archive's eol conversion differs from the checkout's (modules.inc is LF in the tree): the shared files the edits
        # apply to are taken from the working tree, where they are unmodified against HEAD (2b_apply_shared.py --prove checks)
        for p in ("port/src/sim/modules.inc", "port/include/portsim_c.h", "port/src/portsim_c.cpp", "port/src/registry.cpp",
                  "UNCERTAINTIES.md", "CLAUDE.md", "port/CORE.md"):
            st = subprocess.run(["git", "status", "--porcelain", "--", p], cwd=ROOT, capture_output=True, text=True, check=True)
            assert not st.stdout.strip(), f"{p} differs from HEAD"
            shutil.copy2(ROOT / p, DST / p)
        r = subprocess.run([sys.executable, str(HERE / "2b_apply_shared.py"), str(DST)], capture_output=True, text=True)
        (HERE / "2b_fulltree_apply.log").write_text(r.stdout + r.stderr)
        print(r.stdout[-400:])
        if r.returncode:
            sys.exit("apply_shared failed")


if __name__ == "__main__":
    main()
