"""P9C mirror: port/build/P9C = `git archive c5e66b59` (P9 merged, log/ not extracted) + junctions to the shared data.

    ~/bin/py -3.12 log/roadmap/P9/P9C_mirror.py      # refuses an existing mirror
"""
from __future__ import annotations

import pathlib
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[3]
BASE = "c5e66b59"
M = ROOT / "port" / "build" / "P9C"
JUNCTIONS = ["extracted", "log", "original", "port/corpus", "re/replay", "re/versions", "shim/build", "tests/fixtures",
             "tests/golden", "tools/dxsdk8", "tools/vc6", "tools/vc6sp2", "tools/vc6sp5"]


def junction(dst: pathlib.Path, src: pathlib.Path) -> None:
    if not src.exists():
        print("skip (no source)", src)
        return
    if dst.exists():
        shutil.rmtree(dst)
    dst.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["cmd", "/c", "mklink", "/J", str(dst), str(src)], check=True, capture_output=True)
    print("junction", dst.relative_to(M))


def main() -> None:
    assert not M.exists(), M
    M.mkdir(parents=True)
    arc = subprocess.run(["git", "-C", str(ROOT), "archive", BASE, "--", ".", ":(exclude)log"],
                         check=True, capture_output=True).stdout
    subprocess.run(["tar", "-x", "-C", str(M)], input=arc, check=True)
    (M / "HEAD_COMMIT").write_text(BASE + " (P9 merged; P9C base)\n")
    for j in JUNCTIONS:
        junction(M / j, ROOT / j)


if __name__ == "__main__":
    main()
