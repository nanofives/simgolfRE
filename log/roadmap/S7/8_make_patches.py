"""S7.8: the patches of this step against S7.5's accepted mirror (port/build/S7_5_head), from port/build/S7_8_head.

    ~/bin/py -3.12 log/roadmap/S7/8_make_patches.py

Writes log/roadmap/S7/8_tick.patch (the shared tick.cpp / tick_hook.h hunks, for the S7 lead) and 8_step.patch (every
step-owned file: live, app, tests), both `git diff --no-index` style with a/ b/ prefixes relative to the repo root, and
checks that the mirror differs from S7.5's in exactly these files.
"""
from __future__ import annotations

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
A = ROOT / "port" / "build" / "S7_5_head"
B = ROOT / "port" / "build" / "S7_8_head"
OUT = ROOT / "log" / "roadmap" / "S7"
SHARED = ["port/src/sim/tick.cpp", "port/src/sim/tick_hook.h"]
STEP = ["port/include/portsim/sim/live.h", "port/src/sim/live.cpp", "port/app/host.h", "port/app/host.cpp", "port/app/main.cpp",
        "port/test/test_app_ingame.py", "port/test/test_app_live.py", "port/test/test_app_present.py"]


def diff(rel: str) -> str:
    a = f"{A.name}/{rel}" if (A / rel).exists() else "/dev/null"
    args = ["git", "diff", "--no-index", "--no-color", a, f"{B.name}/{rel}"]
    r = subprocess.run(args, capture_output=True, text=True, cwd=str(A.parent))
    return r.stdout.replace(f"a/{A.name}/", "a/").replace(f"b/{B.name}/", "b/").replace(f"a/{B.name}/", "a/")


def changed() -> list[str]:
    out = []
    for d in ("port/src", "port/include", "port/app", "port/test", "port/CMakeLists.txt"):
        for p in sorted((B / d).rglob("*") if (B / d).is_dir() else [B / d]):
            if p.is_file() and "__pycache__" not in p.parts and p.suffix not in (".orig", ".pyc"):
                rel = p.relative_to(B).as_posix()
                q = A / rel
                if not q.exists() or q.read_bytes() != p.read_bytes():
                    out.append(rel)
    return out


if __name__ == "__main__":
    (OUT / "8_tick.patch").write_text("".join(diff(r) for r in SHARED), newline="\n")
    (OUT / "8_step.patch").write_text("".join(diff(r) for r in STEP), newline="\n")
    ch = changed()
    extra = sorted(set(ch) - set(SHARED) - set(STEP))
    missing = sorted(set(SHARED + STEP) - set(ch))
    print("changed:", len(ch), "unexpected:", extra, "unchanged:", missing)
    sys.exit(1 if extra or missing else 0)
