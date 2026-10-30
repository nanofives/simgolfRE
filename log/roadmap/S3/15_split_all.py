"""S3.15: per census FAIL row, how many differing px of each in-game region are terrain-origin and how many 2D-made, with GlRaster
(the default backend), one boot per scenario.

Method `exit` (scenarios with an S3.2 terrain recording log/p5/terrain/<label>): framegate's boot with terrainLive.keepFrame(k) for
each checkpoint's recorded updateTerrainRender ordinal k; per differing pixel of the final frame (port vs the original's back buffer):
  T   the terrain exits already differ (port's root at updateTerrainRender's return vs the original's exit.bin), the pixel unchanged
      after the terrain on both sides;
  TB  the terrain exits differ and a 2D body drew over the pixel afterwards (on either side);
  B   the terrain exits are equal: the difference is made after the terrain (2D-made).
Method `hostgl` (no recording): a second boot with the host-GL terrain (gl_wgl.h, the original's own implementation); a pixel that
differs with GlRaster and is equal with the host GL is terrain-origin (T + TB), one that differs in both is 2D-made (B).

    cd port/build/S3_15 && SG_PORTSIM_DLL=... ~/bin/py -3.12 ../../../log/roadmap/S3/15_split_all.py [--scenarios a,b] \
        --out ../../../log/roadmap/S3/15_split_rows.tsv
"""
import argparse
import csv
import json
import os
import pathlib
import sys
import time

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import framegate as FG  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import terrainlive as TL  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[3]
CENSUS = ROOT / "log" / "roadmap" / "merges" / "I14" / "census.tsv"
W, H = 800, 600
REG = ("hud_title", "hud_meters", "hud_message", "toolbar", "bottom_panel", "view")


def rgb(a):
    return np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=-1).astype(int)


def label_of(s):
    labs = set(os.listdir(ROOT / "log" / "p5" / "terrain"))
    for l in ("s32_" + s, "s32c_" + s, "s32p_" + s, "s32q_" + s) + (("s32_sandbox",) if s == "sandbox_basic" else ()):
        if l in labs:
            return l
    return None


def boot(lib, scenario, chks, keep, host_gl=False):
    """framegate's run_scenario with the final images captured and, for `keep` (frame ordinals), the port's terrain exits"""
    imgs, exits = {}, {}
    orig_rn, orig_run = FG.region_numbers, FG.B.run
    order = []

    def rn(p, r, kd):
        order.append((p.copy(), r.copy()))
        return orig_rn(p, r, kd)

    def run(scenario_, lib_, max_records, before, after, to_end):
        def before2(st):
            before(st)
            for k in keep:
                st.call("terrainLive.keepFrame", [k])

        def after2(st, res):
            for k in keep:
                raw = st.files_read(f"$gfx/terrain_{k}_exit.555")
                if raw is not None and len(raw) == W * H * 2:
                    exits[k] = np.frombuffer(raw, "<u2").reshape(H, W).copy()
            after(st, res)
        return orig_run(scenario_, lib_, max_records=max_records, before=before2, after=after2, to_end=to_end)

    FG.region_numbers, FG.B.run = rn, run
    try:
        rows = FG.run_scenario(scenario, chks, lib=lib, probe=False, host_gl=host_gl)
    finally:
        FG.region_numbers, FG.B.run = orig_rn, orig_run
    it = iter(order)
    for r in rows:
        if r.get("regions") and r.get("status") in ("PASS", "FAIL", "UNSUPPORTED"):
            imgs[r["checkpoint"]] = next(it)
    return rows, imgs, exits


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenarios")
    ap.add_argument("--out", type=pathlib.Path, required=True)
    a = ap.parse_args()
    census = [r for r in csv.DictReader(open(CENSUS, encoding="utf-8"), delimiter="\t") if r["status"] == "FAIL"]
    by = {}
    for r in census:
        by.setdefault(r["scenario"], []).append(r["checkpoint"])
    names = a.scenarios.split(",") if a.scenarios else sorted(by)
    lib = L.load()
    if getattr(lib, "dll", None) is not None:
        TL.bind(lib.dll)
    lab_map, _ = FG.label_map("ingame")
    done = set()
    if a.out.exists():
        done = {r["scenario"] for r in csv.DictReader(open(a.out, encoding="utf-8"), delimiter="\t")}
    new = not a.out.exists()
    f = open(a.out, "a", encoding="utf-8", newline="\n")
    if new:
        f.write("\t".join(["scenario", "checkpoint", "method", "frame"] + [f"{n}_{c}" for n in REG for c in ("px", "T", "TB", "B")]) + "\n")
    for i, s in enumerate(names, 1):
        if s in done:
            continue
        chks = by.get(s, [])
        t0 = time.time()
        lab = label_of(s)
        rec = TL.recorded_frames(lab) if lab else {}
        frame = {c: rec[c]["meta"]["frame"] for c in chks if c in rec}
        method = "exit" if frame else "hostgl"
        rows, imgs, exits = boot(lib, s, chks, sorted(set(frame.values())) if method == "exit" else [])
        if method == "hostgl":
            _r2, imgs2, _ = boot(lib, s, chks, [], host_gl=True)
        n_ok = 0
        for c in chks:
            if c not in imgs:
                continue
            port, ref = imgs[c]
            P, R = port.astype(int) >> 3, ref.astype(int) >> 3
            diff = np.abs(P - R).max(2) > 0
            if method == "exit":
                k = frame.get(c)
                if k is None or k not in exits:
                    continue
                OE, PE = rgb(rec[c]["exit"]), rgb(exits[k])
                ed = (OE != PE).any(2)
                hud = (R != OE).any(2) | (P != PE).any(2)
                T, TB, B = diff & ed & ~hud, diff & ed & hud, diff & ~ed
            else:
                if c not in imgs2:
                    continue
                P2 = imgs2[c][0].astype(int) >> 3
                d2 = np.abs(P2 - R).max(2) > 0
                T, TB, B = diff & ~d2, np.zeros_like(diff), diff & d2
            cells = []
            for j, n in enumerate(REG):
                m = lab_map == j
                cells += [int((diff & m).sum()), int((T & m).sum()), int((TB & m).sum()), int((B & m).sum())]
            f.write("\t".join([s, c, method, str(frame.get(c, ""))] + [str(x) for x in cells]) + "\n")
            n_ok += 1
        f.flush()
        print(f"[{i}/{len(names)}] {s} {method} rows {n_ok}/{len(chks)} ({time.time() - t0:.1f} s)", flush=True)
    f.close()


if __name__ == "__main__":
    main()
