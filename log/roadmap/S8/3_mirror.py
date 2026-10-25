"""S8.3 private build tree (log/roadmap/S8/3_notes.md): HEAD export + this step's files + the merge lines of 3_merge.md.

    ~/bin/py -3.12 log/roadmap/S8/3_mirror.py [--variant merged|module|head]

Root port/build/S8_3_root (gitignored). First run: `git archive HEAD` extracted there, plus junctions to the shared tree's
gitignored data (original, log, port/corpus, tests/golden, tests/fixtures), as log/roadmap/S0/export.md does. Every run copies
this step's owned files in and writes the shared files the variant needs:
  head     the committed modules.inc / tick.cpp / tick_boot.inc (the baseline: the S8.3 module absent)
  module   modules.inc + SG_MODULE(video), tick files as committed (playAudioFile still a drop: the module is unreachable)
  merged   module + 3_merge.md's tick.cpp (playAudioFile no longer a drop) and tick_boot.inc (the intro call) lines
Build: cmake -S port/build/S8_3_root/port -B port/build/S8_3_root/port/build/S8_3 -A x64 (and S8_3_w32 -A Win32).
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
DST = ROOT / "port" / "build" / "S8_3_root"
OWNED = ["port/src/sim/video.cpp", "port/include/portsim/sim/video.h", "port/test/test_video.py", "port/test/golden_video.json",
         "re/tools/s8_boot_capture.py"]
JUNCTIONS = ["original", "log", "port/corpus", "tests/golden", "tests/fixtures"]

# 3_merge.md, items 1-3 (exact text)
MODULE_LINE = "SG_MODULE(video)"
DROP_LINE = '    {"playAudioFile", "the closing Bink video (binkw32); its pumps dispatch no recorded input in the corpus"},\n'
BOOT_ANCHOR = '    cc("showTitleScreen");\n'
BOOT_INSERT = ('    // gameMain+0x41f showTitleScreen plays the intro first (its call at showTitleScreen+0x27, immediate 0): the video\n'
               '    // module (S8.3), whose default host takes the original\'s own skip path (BinkOpen NULL)\n'
               '    cc("playAudioFile", (GS(f_strpool_4c89ac) + 96), 0);           // ./flics/SMSG_introfinal.bik\n')


def head(rel: str) -> bytes:
    return subprocess.run(["git", "show", "HEAD:" + rel], cwd=ROOT, capture_output=True, check=True).stdout


def write(rel: str, data: bytes):
    d = DST / rel
    d.parent.mkdir(parents=True, exist_ok=True)
    if not d.exists() or d.read_bytes() != data:
        d.write_bytes(data)
        return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--variant", choices=("merged", "module", "head"), default="merged")
    a = ap.parse_args()
    if not (DST / "port" / "CMakeLists.txt").exists():
        DST.mkdir(parents=True, exist_ok=True)
        tar = subprocess.run(["git", "archive", "HEAD"], cwd=ROOT, capture_output=True, check=True).stdout
        subprocess.run(["tar", "-xf", "-"], cwd=DST, input=tar, check=True)
        (DST / "HEAD_COMMIT").write_text(subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True,
                                                        check=True).stdout)
    for j in JUNCTIONS:
        link = DST / j
        if link.exists() and not os.path.isjunction(link):
            # the archive's copy of a tracked part of that directory (inside DST only; never rmtree a junction: it would
            # delete the shared tree's files)
            assert DST in link.resolve().parents, link
            shutil.rmtree(link)
        if not link.exists():
            link.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(ROOT / j)], check=True, capture_output=True)
    changed = []
    for rel in OWNED:
        src = ROOT / rel
        if src.exists() and write(rel, src.read_bytes()):
            changed.append(rel)
    mods = head("port/src/sim/modules.inc").decode()
    tick = head("port/src/sim/tick.cpp").decode()
    boot = head("port/src/sim/tick_boot.inc").decode()
    if a.variant in ("module", "merged"):
        mods = mods.rstrip("\n") + "\n" + MODULE_LINE + "\n"
    if a.variant == "merged":
        assert tick.count(DROP_LINE) == 1, "tick.cpp: the playAudioFile drop line is not there once"
        tick = tick.replace(DROP_LINE, "")
        assert boot.count(BOOT_ANCHOR) == 1, "tick_boot.inc: the showTitleScreen call is not there once"
        boot = boot.replace(BOOT_ANCHOR, BOOT_INSERT + BOOT_ANCHOR)
    for rel, text in (("port/src/sim/modules.inc", mods), ("port/src/sim/tick.cpp", tick), ("port/src/sim/tick_boot.inc", boot)):
        if write(rel, text.encode()):
            changed.append(rel)
    if "port/src/sim/modules.inc" in changed:
        (DST / "port" / "src" / "registry.cpp").touch()   # MSBuild does not rebuild it when only modules.inc changes
    print(f"{DST} variant {a.variant}: changed {changed or 'nothing'}")


if __name__ == "__main__":
    sys.exit(main())
