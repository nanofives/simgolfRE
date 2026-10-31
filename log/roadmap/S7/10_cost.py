"""S7.10: the shadow's real-time cost per timer tick (the timer client's call, port/app/audio_out.cpp fire()), x64 (the software
80-bit mixer) and Win32 (real x87), from simgolf_audio_check's stats: the device run (178 sounds started, 40 ticks) and the
title script (340 ticks). Three runs each; prints a TSV row per run and writes log/roadmap/S7/10_cost.tsv.

    ~/bin/py -3.12 log/roadmap/S7/10_cost.py
"""
import json
import os
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
B = ROOT / "port" / "build" / "S7_10_root" / "port" / "build"
EXE, INST = ROOT / "original" / "golf_clean.exe", ROOT / "original"
IMAGE = ROOT / "port" / "corpus" / "segments" / "sandbox_basic" / "00" / "start.data.bin"
SCRIPT = ROOT / "log" / "roadmap" / "S7" / "10_title.script"


def run(arch, kind, k):
    exe = B / arch / "app" / "Release" / "simgolf_audio_check.exe"
    st = pathlib.Path(tempfile.gettempdir()) / f"s710_cost_{arch}_{kind}_{k}.json"
    if kind == "device":
        args = ["device", "--image", str(IMAGE), "--out", os.devnull + ".txt" if False else str(st) + ".txt"]
    else:
        args = ["script", "--script", str(SCRIPT), "--mode", "capture"]
    subprocess.run([str(exe), *args, "--stats", str(st), "--exe", str(EXE), "--install", str(INST)], capture_output=True,
                   check=True)
    s = json.loads(st.read_text())
    return s["ticks"], s["tick_ns_total"] / max(1, s["ticks"]) / 1e6, s["tick_ns_max"] / 1e6


rows = ["arch\tsession\trun\tticks\tmean_ms\tmax_ms"]
for arch in ("x64", "w32"):
    for kind in ("device", "title"):
        for k in range(3):
            n, mean, mx = run(arch, kind, k)
            rows.append(f"{arch}\t{kind}\t{k}\t{n}\t{mean:.3f}\t{mx:.3f}")
            print(rows[-1], flush=True)
(ROOT / "log" / "roadmap" / "S7" / "10_cost.tsv").write_text("\n".join(rows) + "\n")
