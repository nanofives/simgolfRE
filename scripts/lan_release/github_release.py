"""After a LAN publish: make a GitHub release of the same build on the public repo (port files only, no game files).

    ~/bin/py -3.12 scripts/lan_release/github_release.py [--notes <file>] [--dry-run]

Reads the label of port/build/lan_release/manifest.json (`SimGolf port <commit> (<date>)`), checks that the commit is on
origin, zips the port's own files (simgolf_port.exe, freetype.dll, the VC runtime DLLs, a Play SimGolf.bat that takes the
player's own install, README.txt) and runs `gh release create port-<date>-<commit> --target <commit>`. game\\ (the user's
SimGolf install) and the LAN updater (update.bat, simgolf_update.ps1, manifest.json) are never included.
The release is appended to log/roadmap/lan_releases.tsv's sibling log/roadmap/github_releases.tsv.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import time
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
STAGE = ROOT / "port" / "build" / "lan_release"
# P23A item 1: the staged simgolf_port.exe carries the game's icon (LAN only); the release zips the plain build's exe
PLAIN_EXE = ROOT / "port" / "build" / "lan_build" / "port" / "build" / "x64" / "app" / "Release" / "simgolf_port.exe"
PORT_FILES = ["simgolf_port.exe", "freetype.dll", "msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"]

BAT = r"""@echo off
rem SimGolf port. Put this folder next to nothing in particular; point it at YOUR OWN SimGolf install:
rem   set SIMGOLF_INSTALL=C:\Games\SimGolf   (the folder that holds golf.exe and the game's data)
rem Saves go to %LOCALAPPDATA%\SimGolfPort.
if "%SIMGOLF_INSTALL%"=="" (
  echo Set SIMGOLF_INSTALL to your SimGolf install folder first, for example:
  echo   set SIMGOLF_INSTALL=C:\Games\SimGolf
  pause
  exit /b 1
)
start "" /D "%~dp0." "%~dp0simgolf_port.exe" --install "%SIMGOLF_INSTALL%"
"""

README = """SimGolf port {commit} ({date})
==================================

A reimplementation of Sid Meier's SimGolf (v1.03) built from the reverse-engineering project at
https://github.com/nanofives/simgolfRE. It plays from YOUR OWN copy of the game's data files and loads none of the
original binaries. No game files are included here: you need an installed SimGolf (v1.03).

Run:    set SIMGOLF_INSTALL=<your SimGolf folder>, then double-click "Play SimGolf.bat"
        (or: simgolf_port.exe --install <your SimGolf folder>)
Saves:  %LOCALAPPDATA%\\SimGolfPort
Options: listed at the head of port/app/main.cpp in the repository (for example --wide off, --profile original).

This build uses FreeType (https://freetype.org), portions of this software are copyright (c) The FreeType Project
(www.freetype.org). All rights reserved. The Microsoft Visual C++ runtime DLLs are redistributed as permitted.
SimGolf is a trademark of its owners; this project is not affiliated with them.
"""


def sh(cmd, **kw):
    return subprocess.run(cmd, cwd=ROOT, text=True, capture_output=True, **kw)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--notes", type=pathlib.Path, help="release notes file (markdown); default: the commit subject")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    label = json.loads((STAGE / "manifest.json").read_text())["label"]
    m = re.match(r"SimGolf port (\w+) \((\d{4}-\d{2}-\d{2})\)", label)
    if not m:
        sys.exit(f"unexpected manifest label: {label!r}")
    commit, date = m.groups()
    full = sh(["git", "rev-parse", commit]).stdout.strip()
    if sh(["git", "merge-base", "--is-ancestor", full, "origin/main"]).returncode != 0:
        sys.exit(f"{commit} is not on origin/main: push first")
    def source(f):
        return PLAIN_EXE if f == "simgolf_port.exe" else STAGE / f

    for f in PORT_FILES:
        if not source(f).is_file():
            sys.exit(f"missing {source(f)}")
    import pefile  # the public zip never carries the game's icon (RT_GROUP_ICON = 14)
    pe = pefile.PE(str(PLAIN_EXE), fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    if any(e.id == 14 for e in getattr(getattr(pe, "DIRECTORY_ENTRY_RESOURCE", None), "entries", [])):
        sys.exit(f"{PLAIN_EXE} carries an icon resource: the GitHub zip takes the plain build only")
    tag = f"port-{date}-{commit}"
    out = pathlib.Path(tempfile.mkdtemp(prefix="ghrel_")) / f"simgolf_port_{date}_{commit}.zip"
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for f in PORT_FILES:
            z.write(source(f), f"simgolf_port/{f}")
        z.writestr("simgolf_port/Play SimGolf.bat", BAT.replace("\n", "\r\n"))
        z.writestr("simgolf_port/README.txt", README.format(commit=commit, date=date).replace("\n", "\r\n"))
        assert not any(n.startswith("simgolf_port/game") for n in z.namelist())
    notes = a.notes.read_text() if a.notes else sh(["git", "log", "-1", "--format=%s", full]).stdout.strip()
    cmd = ["gh", "release", "create", tag, str(out), "--target", full, "--title", f"SimGolf port {date} ({commit})",
           "--notes", notes]
    print(" ".join(cmd[:4]), out, f"{out.stat().st_size:,} B")
    if a.dry_run:
        return 0
    r = sh(cmd)
    print(r.stdout.strip() or r.stderr.strip())
    if r.returncode != 0:
        return r.returncode
    with open(ROOT / "log" / "roadmap" / "github_releases.tsv", "a", encoding="utf-8") as fh:
        fh.write(f"{time.strftime('%Y-%m-%d %H:%M:%S')}\t{commit}\t{tag}\t{r.stdout.strip()}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
