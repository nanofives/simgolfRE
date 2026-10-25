"""S2.8: write log/roadmap/S2/8_shared.patch = the mirror port/build/S2_8_head against port/build/S2_4_head for the shared
files this step edits, without the mirror-only boot.py KNOWN entry (U-1253, the lead's decision 2026-10-24) (unified diff, a/ b/ repo paths, LF). New files of the step are copied by 8_merge.md, not patched.

usage: ~/bin/py -3.12 log/roadmap/S2/8_tools/make_patch.py
"""
import difflib
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[4]
BASE = ROOT / "port" / "build" / "S2_4_head"
MIR = ROOT / "port" / "build" / "S2_8_head"
FILES = [
    "port/src/sim/gfx_families.inc",
    "port/src/sim/gfx_body.h",
    "port/src/sim/gfx.cpp",
    "port/src/sim/commands.cpp",
    "port/src/sim/commands_screens.inc",
    "port/src/sim/holestats.cpp",
    "port/src/sim/routing.cpp",
    "port/src/sim/reports.cpp",
    "port/test/framegate.py",
]


def main():
    out = []
    for f in FILES:
        a = (BASE / f).read_text(encoding="utf-8").replace("\r\n", "\n").splitlines(keepends=True)
        b = (MIR / f).read_text(encoding="utf-8").replace("\r\n", "\n").splitlines(keepends=True)
        d = list(difflib.unified_diff(a, b, fromfile="a/" + f, tofile="b/" + f, n=3))
        if d:
            out.append(f"diff --git a/{f} b/{f}\n")
            out.extend(d)
            print(f, sum(1 for x in d if x.startswith("@@")), "hunks")
    p = ROOT / "log" / "roadmap" / "S2" / "8_shared.patch"
    p.write_bytes("".join(out).encode("utf-8"))
    print(p, p.stat().st_size, "bytes")


if __name__ == "__main__":
    main()
