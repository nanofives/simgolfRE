"""Bring publish.sh's build folder to exactly `git archive <commit> <paths>`, writing only the files whose bytes differ and
deleting source files the commit no longer has, so the build after it recompiles only what changed (Mariano 2026-11-09: a
release took 457 s, most of it a clean build). Uncommitted work still never ships: every source byte comes from the archive.
Build outputs (port/build/ inside the folder) are left alone. Written files get the current time, so MSBuild always sees them
as newer than their objects whatever the commit's date.

    py -3.12 scripts/lan_release/sync_build_tree.py <build dir> <commit> <path>...
"""
from __future__ import annotations

import io
import os
import pathlib
import subprocess
import sys
import tarfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUTPUTS = "port/build/"


def is_link(p: str) -> bool:
    """A junction, symbolic link or any other reparse point: never walked, never deleted through. On 2026-11-18 a worker ran
    this script on a merge tree whose log / extracted / original folders are junctions into the shared repo, and the walk
    deleted 13,646 tracked and every untracked file under log/ (up to a permission error) and all of extracted/."""
    try:
        st = os.lstat(p)
    except OSError:
        return True
    return os.path.islink(p) or bool(getattr(st, "st_file_attributes", 0) & 0x400) or (hasattr(os.path, "isjunction") and os.path.isjunction(p))


def main() -> int:
    build = pathlib.Path(sys.argv[1]).resolve()
    commit, paths = sys.argv[2], sys.argv[3:]
    arc = subprocess.run(["git", "-C", str(ROOT), "archive", commit, "--", *paths], check=True, capture_output=True).stdout
    want: dict[str, bytes] = {}
    with tarfile.open(fileobj=io.BytesIO(arc)) as tf:
        for m in tf.getmembers():
            if m.isfile():
                want[m.name] = tf.extractfile(m).read()
    build.mkdir(parents=True, exist_ok=True)
    # only the archived paths are this script's: a file elsewhere in the folder is never listed, so never deleted
    roots = [p.strip("/").replace("\\", "/") + "/" for p in paths]
    have = set()
    for dirpath, dirnames, filenames in os.walk(build):
        rel_dir = pathlib.Path(dirpath).relative_to(build).as_posix()
        rel_dir = "" if rel_dir == "." else rel_dir + "/"
        dirnames[:] = [d for d in dirnames
                       if not (rel_dir + d + "/").startswith(OUTPUTS)  # never into the build outputs
                       and not is_link(os.path.join(dirpath, d))        # never through a junction or link
                       and any((rel_dir + d + "/").startswith(r) or r.startswith(rel_dir + d + "/") for r in roots)]
        have.update(rel_dir + f for f in filenames if any((rel_dir + f).startswith(r) for r in roots))
    doomed = sorted(have - set(want))
    if len(doomed) > 200 and len(doomed) > len(want) // 2 and os.environ.get("SYNC_BUILD_TREE_FORCE") != "1":
        print(f"[!] refusing: {len(doomed)} files would be removed for {len(want)} in the archive "
              f"(first: {doomed[0]}); set SYNC_BUILD_TREE_FORCE=1 if that is meant")
        return 3
    written = removed = 0
    for name, data in want.items():
        p = build / name
        if name in have and p.read_bytes() == data:
            continue
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
        written += 1
    for name in doomed:
        (build / name).unlink()
        removed += 1
    if removed:  # source folders left empty by the deletions (deepest first; never the outputs or the folders above them)
        dirs = []
        for dirpath, dirnames, _files in os.walk(build):
            rel = pathlib.Path(dirpath).relative_to(build).as_posix() + "/"
            dirnames[:] = [d for d in dirnames if not (("" if rel == "./" else rel) + d + "/").startswith(OUTPUTS)
                           and not is_link(os.path.join(dirpath, d))]
            if rel != "./" and not OUTPUTS.startswith(rel):
                dirs.append(dirpath)
        for d in sorted(dirs, key=len, reverse=True):
            if not os.listdir(d):
                os.rmdir(d)
    print(f"[+] build folder synced to {commit}: {len(want)} source files, {written} written, {removed} removed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
