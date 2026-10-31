"""S7.11 (U-1499): a scratch install equal to original\\ but for the course-sfx directory, staged under the exe's name
`sounds\\Golf sfx` (as the LAN staging change in 11_merge.md does). Directories are junctions and files hard links into
original\\, so original\\ itself is never written. Then audio_stream's device run (simgolf_audio_check device, 300 playSounds)
on both installs: the lines that read a sound file and the buffers made are counted.

    ~/bin/py -3.12 log/roadmap/S7/11_sfx_install.py <scratch dir>
"""
import json
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
ORIG = ROOT / "original"
AC = ROOT / "port/build/S7_11_root/port/build/x64/app/Release/simgolf_audio_check.exe"
IMAGE = ROOT / "port/corpus/segments/sandbox_basic/00/start.data.bin"


def link(src: pathlib.Path, dst: pathlib.Path):
    if src.is_dir():
        subprocess.run(["cmd", "/c", "mklink", "/J", str(dst), str(src)], check=True, capture_output=True)
    else:
        os.link(src, dst)


def build(dst: pathlib.Path):
    if dst.exists():
        subprocess.run(["cmd", "/c", "rmdir", "/s", "/q", str(dst)], capture_output=True)  # junctions: not followed
    (dst / "sounds").mkdir(parents=True)
    for e in ORIG.iterdir():
        if e.name.lower() != "sounds":
            link(e, dst / e.name)
    music = {"desert_music": "Desert Music", "links_music": "Links Music", "tropical_music": "Tropical Music"}  # S7.13 (G16)
    for e in (ORIG / "sounds").iterdir():
        if e.name.lower() == "music":
            (dst / "sounds" / e.name).mkdir()
            for m in e.iterdir():
                link(m, dst / "sounds" / e.name / music.get(m.name.lower(), m.name))
            continue
        link(e, dst / "sounds" / ("Golf sfx" if e.name.lower() == "golf_sfx" else e.name))


def device(install: pathlib.Path, out: pathlib.Path) -> dict:
    lines, st = out.with_suffix(".txt"), out.with_suffix(".json")
    subprocess.run([str(AC), "device", "--exe", str(ORIG / "golf_clean.exe"), "--install", str(install), "--image", str(IMAGE),
                    "--out", str(lines), "--stats", str(st)], capture_output=True, text=True, timeout=600)
    t = lines.read_text().split("\n")
    return {"lines": len(t), "mmioOpen": sum("mmioOpen" in x for x in t), "CreateSoundBuffer": sum(x.startswith("com 1.3 ") for x in t),
            "unlocks_not_silent": sum(", 0 not silent" not in x and "; unlock" in x for x in t)}


if __name__ == "__main__":
    scratch = pathlib.Path(sys.argv[1])
    build(scratch / "inst")
    res = {"original": device(ORIG, scratch / "dev_original"), "golf_sfx_renamed": device(scratch / "inst", scratch / "dev_renamed")}
    print(json.dumps(res, indent=1))
