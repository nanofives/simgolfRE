"""S3.12 (copy of 10b_session.py with the wave 6 trees): one app session of S7.5's in-game script (test_app_ingame.script_ingame: title -> Sandbox -> Monterey, seven arrow-key camera
moves, the zoom keys X Z X Z, a hole built), driven by posted messages only, and the work of every in-game frame that rasterised terrain
passes (waits.tsv: frame, work ms, passes in the frame, pixels in the frame). The recording is kept in log/roadmap/S3/12_rec/<tag>/.

    ~/bin/py -3.12 log/roadmap/S3/12_session.py <12|head> <tag> [--runs N]

12: the wave 6 tree port/build/S3_12 (12_assemble.py); head: port/build/S3_12_head = 01d84188 with I13's gfx_terrain.cpp (the
reference: 01d84188 itself does not compile, 12_notes.md section 1); both built with port/test/build.py --arch x64 (build x64).

Uses the app tree's test module (port/test/test_app_ingame.py of the tree) with that tree's simgolf_port.exe. Prints the
machine load (other python / cl / MSBuild / simgolf_port processes and the CPU load percentage) before each run.
"""
import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
OUT = ROOT / "log" / "roadmap" / "merges" / "I15" / "rec"


def load_snapshot() -> dict:
    ps = ("$p = Get-Process | Where-Object { $_.ProcessName -match '^(python|py|cl|link|MSBuild|simgolf_port|portsim_runner32|"
          "golf_clean)$' } | Group-Object ProcessName | ForEach-Object { \"$($_.Name)=$($_.Count)\" }; "
          "$l = (Get-CimInstance Win32_Processor).LoadPercentage; \"load=$l\"; $p")
    r = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True)
    out = {}
    for ln in r.stdout.split():
        if "=" in ln:
            k, v = ln.split("=", 1)
            out[k] = v
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tree", choices=["12", "head", "e179", "I15"])
    ap.add_argument("tag")
    ap.add_argument("--runs", type=int, default=1)
    a = ap.parse_args()
    tree = ROOT / "port" / "build" / {"12": "S3_12", "I15": "I15", "head": "S3_12_head", "e179": "S3_12_e179"}[a.tree]
    bdir = tree / "port" / "build" / "x64"
    os.environ["SG_APP_EXE"] = str(bdir / "app" / "Release" / "simgolf_port.exe")
    os.environ["SG_PORTSIM_DLL"] = str(bdir / "Release" / "portsim_c.dll")
    sys.path.insert(0, str(tree))
    sys.path.insert(0, str(tree / "port" / "test"))
    os.chdir(tree)
    import test_app_ingame as T  # noqa: E402  (the tree's own copy)
    exe = pathlib.Path(os.environ["SG_APP_EXE"])
    summary = []
    for k in range(a.runs):
        load = load_snapshot()
        OUT.mkdir(parents=True, exist_ok=True)
        s = T.Session(exe, T.script_ingame)
        lo = T._ingame_from(s)
        fp = T._frame_periods(s, lo)
        pix = {s.waits[i][0]: s.waits[i][7] - s.waits[i - 1][7] for i in range(1, len(s.waits)) if len(s.waits[i]) >= 8}
        rows = [dict(r, pixels=pix.get(r["frame"], 0)) for r in fp if r["passes"] > 0]
        over = [r for r in fp if r["waited"] and r["work"] >= 87]
        res = {"run": k, "load_before": load, "error": s.error, "rc": s.rc, "session": s.session.get("session"),
               "terrain_frames": rows, "over_87": [(r["frame"], r["work"], r["passes"]) for r in over],
               "max_work_terrain": max((r["work"] for r in rows if r["work"] is not None), default=None)}
        dst = OUT / f"{a.tag}_{k}"
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(s.rec, dst)
        (dst / "summary.json").write_text(json.dumps(res, indent=1))
        s.cleanup()
        print(f"run {k}: load {load} error {s.error!r} rc {s.rc}", flush=True)
        for r in rows:
            print(f"  frame {r['frame']:4d} passes {r['passes']} px {r['pixels']:7d} work {r['work']} period {r['period']}", flush=True)
        print(f"  over 87 ms: {res['over_87']}  max terrain-frame work {res['max_work_terrain']}", flush=True)
        summary.append(res)
    (OUT / f"{a.tag}.json").write_text(json.dumps(summary, indent=1))
    return 0


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    sys.exit(main())
