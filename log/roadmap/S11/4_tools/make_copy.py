"""S11.4: make port/build/<dst> a copy of port/build/<src> (default I22 -> S11_4) without its build outputs (port/build) and with
every junction / symlink re-created as a junction to the same target (log, original, extracted, port/corpus, tests/golden ...).
Never writes into <src>. Refuses an existing <dst> unless --force-empty-check passes (dst missing).

    ~/bin/py -3.12 log/roadmap/S11/4_tools/make_copy.py [--src I22] [--dst S11_4]
"""
from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]


def is_link(p: pathlib.Path) -> bool:
    return p.is_symlink() or os.path.isjunction(p)


def target(p: pathlib.Path) -> str:
    return os.path.realpath(p)


def main() -> None:
    a = sys.argv
    src = ROOT / "port" / "build" / (a[a.index("--src") + 1] if "--src" in a else "I22")
    dst = ROOT / "port" / "build" / (a[a.index("--dst") + 1] if "--dst" in a else "S11_4")
    assert src.is_dir(), src
    if "--recreate" in a and dst.exists():
        # remove the junctions first (os.rmdir on a junction removes the link, never its target), then the copy
        assert dst.name.startswith("S11_4"), dst
        stack, links = [dst], []
        while stack:                              # walk without entering a link
            for e in stack.pop().iterdir():
                if is_link(e):
                    links.append(e)
                elif e.is_dir():
                    stack.append(e)
        for p in links:
            os.rmdir(p)
            assert not p.exists(), p
        shutil.rmtree(dst)
    assert not dst.exists(), f"{dst} exists"
    skip = {src / "port" / "build"}
    links = []
    nfiles = 0
    stack = [src]
    while stack:
        d = stack.pop()
        rel = d.relative_to(src)
        (dst / rel).mkdir(parents=True, exist_ok=True)
        for e in d.iterdir():
            if e in skip:
                continue
            if is_link(e):
                links.append((e.relative_to(src), target(e)))
            elif e.is_dir():
                stack.append(e)
            else:
                shutil.copy2(e, dst / e.relative_to(src))
                nfiles += 1
    for rel, tgt in links:
        link = dst / rel
        r = subprocess.run(["cmd", "/c", "mklink", "/J", str(link), tgt], capture_output=True, text=True)
        assert r.returncode == 0, (link, tgt, r.stdout, r.stderr)
    (dst / "port" / "build").mkdir(parents=True, exist_ok=True)
    print(f"{nfiles} files, {len(links)} junctions -> {dst}")
    for rel, tgt in links:
        print(f"  {rel} -> {tgt}")


if __name__ == "__main__":
    main()
