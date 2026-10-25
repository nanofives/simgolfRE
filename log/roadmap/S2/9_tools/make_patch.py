"""S2.9: write log/roadmap/S2/9_shared.patch (LF) = the shared-file edits of the mirror port/build/S2_9_head against the
combined S2.1-S2.4 tree (port/build/S2_4_head), and check that it applies there with `git apply --check`.

usage: ~/bin/py -3.12 log/roadmap/S2/9_tools/make_patch.py
"""
import difflib
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[4]
NEW = ROOT / "port" / "build" / "S2_9_head"
BASE = ROOT / "port" / "build" / "S2_4_head"
SHARED = [
    "port/src/sim/gfx_families.inc",
    "port/src/sim/commands_popup.inc",
    "port/src/sim/saveload.cpp",
    "port/src/sim/reports.cpp",
    "port/src/sim/menu.cpp",
]
OUT = ROOT / "log" / "roadmap" / "S2" / "9_shared.patch"


def lines(p: pathlib.Path) -> list[str]:
    return p.read_bytes().decode("utf-8").replace("\r\n", "\n").splitlines(keepends=True)


def main() -> int:
    parts = []
    for rel in SHARED:
        a, b = lines(BASE / rel), lines(NEW / rel)
        if a == b:
            print("unchanged", rel)
            continue
        parts.append(f"diff --git a/{rel} b/{rel}\n")
        parts.extend(difflib.unified_diff(a, b, f"a/{rel}", f"b/{rel}", n=3))
    OUT.write_bytes("".join(parts).encode("utf-8"))
    print("wrote", OUT, len(parts), "lines")
    # check: the base files (LF) in a scratch git tree, then `git apply --check`
    with tempfile.TemporaryDirectory() as td:
        t = pathlib.Path(td)
        for rel in SHARED:
            (t / rel).parent.mkdir(parents=True, exist_ok=True)
            (t / rel).write_bytes("".join(lines(BASE / rel)).encode("utf-8"))
        subprocess.run(["git", "init", "-q"], cwd=t, check=True)
        r = subprocess.run(["git", "apply", "--check", "-v", str(OUT)], cwd=t, capture_output=True, text=True)
        print(r.stdout + r.stderr)
        if r.returncode:
            return r.returncode
        r = subprocess.run(["git", "apply", str(OUT)], cwd=t, capture_output=True, text=True)
        if r.returncode:
            print(r.stdout + r.stderr)
            return r.returncode
        for rel in SHARED:
            if "".join(lines(t / rel)) != "".join(lines(NEW / rel)):   # (git may write CRLF: autocrlf)
                print("MISMATCH after apply", rel)
                return 1
        print("applies; every file equals the mirror's (LF)")
    shutil.rmtree(td, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
