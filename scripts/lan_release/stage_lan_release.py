#!/usr/bin/env python3
r"""stage_lan_release.py - build the LAN release folder of the SimGolf port for the mini-server (owner request
2026-10-29, modelled on TD5RE's td5re-files and SimCity's re/scripts/lan_release).

The folder is what a player PC keeps:
  simgolf_port.exe            the port's Windows app (no original binary is loaded; the install is read as data)
  freetype.dll                optional text provider the app links
  msvcp140.dll vcruntime140.dll vcruntime140_1.dll   the VC++ 2015-2022 runtime, app-local (no installer needed)
  game\                       the game install the app reads (--install): the user's own SimGolf v1.03 files,
                              golf_clean.exe included (read as data for the game's constants). Left out: the SafeDisc
                              exes and driver, the RE shim (winmm.dll and its ini / log), dev logs, JPEG.lib and the
                              saved games of the dev PC.
  Play SimGolf.bat            starts the app on game\ (the updater also makes a "SimGolf (port)" shortcut)
  simgolf_update.ps1, update.bat   the updater (copies of the files beside this script)
  README.txt
  manifest.json               {label, generated, files: [{path, size, sha256, seed}]}

Saves and recordings go to %LOCALAPPDATA%\SimGolfPort on each PC; nothing in the folder is written by the app.
Files are hard-linked from the sources when they share a volume (no extra disk), copied otherwise.
The stage folder lives under port/build (gitignored); never stage into a tracked folder.

  py -3.12 stage_lan_release.py --app <dir with simgolf_port.exe> --install <original> --out <folder> [--label X]
"""
import argparse
import datetime
import glob
import hashlib
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

APP_FILES = ["simgolf_port.exe", "freetype.dll"]
VC_RUNTIME = ["msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll"]
VC_REDIST_GLOB = (r"C:\Program Files*\Microsoft Visual Studio\2022\*\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT")
# install entries that are not game data (lower-case names, top level of original\)
INSTALL_SKIP = {"golf.exe", "golf.v100.safedisc.exe", "golf_v100_clean.exe", "drvmgt.dll", "secdrv.sys",
                "winmm.dll", "simgolf_shim.ini", "simgolf_shim.log", "logfile.txt", "jpeg.lib", "descript.ion",
                "saved games"}
SKIP_SUFFIXES = (".log", ".bak", ".tmp", ".c3ft_main", ".dl_tmp")
CLIENT_FILES = ["simgolf_update.ps1", "update.bat", "Play SimGolf.bat", "README.txt"]
SEED = set()   # nothing per-PC lives in the folder (saves are under %LOCALAPPDATA%\SimGolfPort)


def place(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if os.path.exists(dst):
        os.remove(dst)
    try:
        os.link(src, dst)
    except OSError:
        shutil.copy2(src, dst)


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def stamp_icon(exe, game_exe):
    """P23A item 1 (Mariano: "Game icon, LAN only"): copy the game's RT_GROUP_ICON and its RT_ICON entries from game_exe into
    the STAGED copy `exe` (UpdateResource). Never run on the build tree's exe and never on what github_release.py zips."""
    import ctypes
    from ctypes import wintypes
    import pefile
    pe = pefile.PE(game_exe)
    res = {}  # (type, id) -> (lang, bytes)
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if t.id not in (3, 14):  # RT_ICON, RT_GROUP_ICON
            continue
        for n in t.directory.entries:
            for l in n.directory.entries:
                d = l.data.struct
                res[(t.id, n.id)] = (l.id, pe.get_data(d.OffsetToData, d.Size))
    if not any(k[0] == 14 for k in res):
        sys.exit("[!] no RT_GROUP_ICON in %s" % game_exe)
    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    k32.BeginUpdateResourceW.restype = wintypes.HANDLE
    k32.BeginUpdateResourceW.argtypes = [wintypes.LPCWSTR, wintypes.BOOL]
    k32.UpdateResourceW.argtypes = [wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p, wintypes.WORD, ctypes.c_void_p, wintypes.DWORD]
    k32.EndUpdateResourceW.argtypes = [wintypes.HANDLE, wintypes.BOOL]
    h = k32.BeginUpdateResourceW(exe, False)
    if not h:
        sys.exit("[!] BeginUpdateResource %s: error %d" % (exe, ctypes.get_last_error()))
    keep = []
    for (t, i), (lang, data) in sorted(res.items()):
        buf = ctypes.create_string_buffer(data, len(data))
        keep.append(buf)
        if not k32.UpdateResourceW(h, t, i, lang, buf, len(data)):
            k32.EndUpdateResourceW(h, True)
            sys.exit("[!] UpdateResource %d/%d: error %d" % (t, i, ctypes.get_last_error()))
    if not k32.EndUpdateResourceW(h, False):
        sys.exit("[!] EndUpdateResource: error %d" % ctypes.get_last_error())
    print("[+] stamped %d icon resources from %s into %s" % (len(res), os.path.basename(game_exe), os.path.basename(exe)))


def vc_redist_dir():
    hits = sorted(glob.glob(VC_REDIST_GLOB))
    for d in reversed(hits):
        if all(os.path.isfile(os.path.join(d, f)) for f in VC_RUNTIME):
            return d
    sys.exit("[!] no VC143 x64 redist folder with %s found under %s" % (", ".join(VC_RUNTIME), VC_REDIST_GLOB))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", required=True, help="folder holding simgolf_port.exe and freetype.dll")
    ap.add_argument("--install", required=True, help="the SimGolf v1.03 install (original\\)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--label", default="")
    a = ap.parse_args()

    out = os.path.abspath(a.out)
    if os.path.isdir(os.path.join(out, ".git")):
        sys.exit("[!] refusing to stage into a git checkout: %s" % out)
    os.makedirs(out, exist_ok=True)
    wanted = set()

    for f in APP_FILES:
        src = os.path.join(a.app, f)
        if not os.path.isfile(src):
            sys.exit("[!] missing %s" % src)
        place(src, os.path.join(out, f)); wanted.add(f)
    # P23A item 1: the staged exe gets the game's icon. It is a COPY (place() may hardlink the build's exe, which stays plain:
    # github_release.py zips the build's exe, not this one).
    staged_exe = os.path.join(out, "simgolf_port.exe")
    os.remove(staged_exe)
    shutil.copy2(os.path.join(a.app, "simgolf_port.exe"), staged_exe)
    game_exe = os.path.join(a.install, "golf_clean.exe")
    if not os.path.isfile(game_exe):
        sys.exit("[!] %s has no golf_clean.exe" % a.install)
    stamp_icon(staged_exe, game_exe)
    vc = vc_redist_dir()
    for f in VC_RUNTIME:
        place(os.path.join(vc, f), os.path.join(out, f)); wanted.add(f)

    if not os.path.isfile(os.path.join(a.install, "golf_clean.exe")):
        sys.exit("[!] %s has no golf_clean.exe" % a.install)
    for top in sorted(os.listdir(a.install)):
        if top.lower() in INSTALL_SKIP:
            continue
        src_top = os.path.join(a.install, top)
        if os.path.isfile(src_top):
            pairs = [(src_top, top)]
        else:
            pairs = []
            for root, dirs, files in os.walk(src_top):
                dirs.sort()
                for fn in sorted(files):
                    full = os.path.join(root, fn)
                    pairs.append((full, os.path.relpath(full, a.install)))
        for src, rel in pairs:
            if rel.lower().endswith(SKIP_SUFFIXES):
                continue
            dst_rel = os.path.join("game", rel)
            place(src, os.path.join(out, dst_rel)); wanted.add(dst_rel)

    for f in CLIENT_FILES:
        place(os.path.join(HERE, f), os.path.join(out, f)); wanted.add(f)

    # drop what an earlier stage left that this one no longer ships (never the manifest)
    wanted_norm = {w.replace("\\", "/").lower() for w in wanted}
    for root, dirs, files in os.walk(out):
        for fn in files:
            rel = os.path.relpath(os.path.join(root, fn), out).replace("\\", "/")
            if rel.lower() != "manifest.json" and rel.lower() not in wanted_norm:
                os.remove(os.path.join(root, fn))

    entries = []
    for rel in sorted(wanted, key=lambda s: s.lower()):
        p = os.path.join(out, rel)
        r = rel.replace("\\", "/")
        e = {"path": r, "size": os.path.getsize(p), "sha256": sha256(p)}
        if r.lower() in SEED:
            e["seed"] = True
        entries.append(e)
    man = {"label": a.label or "SimGolf port", "generated": datetime.datetime.now().strftime("%Y-%m-%d %H:%M"),
           "files": entries}
    with open(os.path.join(out, "manifest.json"), "w", encoding="utf-8", newline="\n") as fh:
        json.dump(man, fh, indent=1)
    print("[+] staged %d files, %.1f MB -> %s" % (len(entries), sum(e["size"] for e in entries) / 1e6, out))


if __name__ == "__main__":
    main()
