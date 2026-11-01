"""S3.22 (W8.3): per census FAIL row with the host-GL terrain (census_auto.tsv), the differing pixels as components with boxes,
a before / after-the-sprite-queue split from the original's root at renderSpriteQueue's return (port/corpus/scene), and a
montage image port | original | differing px.

    cd port/build/S3_22 && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/22_rows.py [--scenarios a,b] \
        --out ../../../log/roadmap/S3/22_rows.tsv --img ../../../log/roadmap/S3/22_img

One framegate boot per scenario with host_gl=True (the terrain is exact), the rows' FAIL checkpoints only. Columns:
  px        differing px (any 5-bit channel difference), beyond  px with a channel difference >= 2
  q_after   differing px where the original's final frame differs from its root at renderSpriteQueue's return (drawn after
            the queue: HUD, text, overlays), q_before the others (terrain / sprite queue); q_na when no scene root exists
  comps     components (8-px proximity): n:x0,y0,x1,y1:px:maxdiff, largest first
"""
import argparse
import csv
import json
import os
import pathlib
import sys
import zlib

import numpy as np

sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import framegate as FG  # noqa: E402
from port.test import gfx as GX  # noqa: E402
from port.test import lib as L  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[3]
CENSUS = ROOT / "log" / "roadmap" / "S3" / "20_out" / "S3_20" / "census_auto.tsv"
SCENE = ROOT / "port" / "corpus" / "scene"


def comps(mask, gap=8, limit=12):
    ys, xs = np.nonzero(mask)
    pts = list(zip(xs.tolist(), ys.tolist()))
    par = list(range(len(pts)))

    def find(i):
        while par[i] != i:
            par[i] = par[par[i]]
            i = par[i]
        return i
    cell = {}
    for i, (x, y) in enumerate(pts):
        cell.setdefault((x // gap, y // gap), []).append(i)
    for (cx, cy), idxs in cell.items():
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for j in cell.get((cx + dx, cy + dy), []):
                    for i in idxs:
                        if i < j and abs(pts[i][0] - pts[j][0]) <= gap and abs(pts[i][1] - pts[j][1]) <= gap:
                            par[find(i)] = find(j)
    g = {}
    for i in range(len(pts)):
        g.setdefault(find(i), []).append(i)
    out = []
    for idxs in g.values():
        px = [pts[i] for i in idxs]
        out.append((min(p[0] for p in px), min(p[1] for p in px), max(p[0] for p in px), max(p[1] for p in px), len(px)))
    out.sort(key=lambda c: -c[4])
    return out[:limit], len(out)


def rgb5(a):
    return a.astype(int) >> 3


def scene_root(scen, chk):
    d = SCENE / scen
    try:
        idx = json.loads((d / "index.json").read_text())
        m = idx["checkpoints"].get(chk)
        if not m or m.get("none") or not (d / f"{chk}.z").exists():
            return None
        raw = np.frombuffer(zlib.decompress((d / f"{chk}.z").read_bytes()), "<u2").reshape(FG.H, FG.W)
        return np.stack([(raw >> 10) & 31, (raw >> 5) & 31, raw & 31], axis=-1).astype(int)
    except Exception:
        return None


def montage(port, ref, diff, box, path):
    from PIL import Image
    x0, y0, x1, y1 = box
    x0, y0 = max(0, x0 - 24), max(0, y0 - 24)
    x1, y1 = min(FG.W, x1 + 25), min(FG.H, y1 + 25)
    sc = max(1, min(4, 360 // max(1, max(x1 - x0, y1 - y0))))
    d = np.zeros((FG.H, FG.W, 3), np.uint8)
    d[diff] = (255, 255, 255)
    tiles = [np.asarray(a)[y0:y1, x0:x1] for a in (port, ref, d)]
    tiles = [np.kron(t, np.ones((sc, sc, 1), np.uint8)) for t in tiles]
    sep = np.full((tiles[0].shape[0], 4, 3), 128, np.uint8)
    Image.fromarray(np.concatenate([tiles[0], sep, tiles[1], sep, tiles[2]], axis=1)).save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scenarios")
    ap.add_argument("--out", type=pathlib.Path, required=True)
    ap.add_argument("--img", type=pathlib.Path, required=True)
    a = ap.parse_args()
    fails = [r for r in csv.DictReader(open(CENSUS, encoding="utf-8"), delimiter="\t") if r["status"] == "FAIL"]
    by = {}
    for r in fails:
        by.setdefault(r["scenario"], []).append(r["checkpoint"])
    names = a.scenarios.split(",") if a.scenarios else sorted(by)
    a.img.mkdir(parents=True, exist_ok=True)
    done = set()
    if a.out.exists():
        done = {r["scenario"] for r in csv.DictReader(open(a.out, encoding="utf-8"), delimiter="\t")}
    new = not a.out.exists()
    f = open(a.out, "a", encoding="utf-8", newline="\n")
    if new:
        f.write("scenario\tcheckpoint\tpx\tbeyond\tq_before\tq_after\tcomps\n")
    lib = L.load()
    for s in names:
        if s in done:
            continue
        order, orig = [], FG.region_numbers

        def rn(p_, r_, kd, _o=orig, _l=order):
            _l.append(p_.copy())
            return _o(p_, r_, kd)
        FG.region_numbers = rn
        try:
            rows = FG.run_scenario(s, by[s], lib=lib, probe=False, host_gl=True)
        finally:
            FG.region_numbers = orig
        it, sink = iter(order), {}
        for r in rows:
            if r.get("regions") and r.get("status") in ("PASS", "FAIL", "UNSUPPORTED"):
                sink[r["checkpoint"]] = next(it)
        for c in by[s]:
            img = sink.get(c)
            if img is None:
                f.write(f"{s}\t{c}\t-1\t-1\t-1\t-1\tno snapshot\n")
                continue
            ref = GX.reference(s, c)
            P, R = rgb5(img), rgb5(ref)
            dd = np.abs(P - R).max(2)
            diff = dd > 0
            q = scene_root(s, c)
            if q is None:
                qb, qa = "na", "na"
            else:
                after = (R != q).any(2)
                qa, qb = int((diff & after).sum()), int((diff & ~after).sum())
            cs, n = comps(diff)
            cstr = ";".join(f"{i}:{x0},{y0},{x1},{y1}:{px}:{int(dd[y0:y1 + 1, x0:x1 + 1].max())}" for i, (x0, y0, x1, y1, px) in enumerate(cs))
            if n > len(cs):
                cstr += f";+{n - len(cs)} more"
            f.write(f"{s}\t{c}\t{int(diff.sum())}\t{int((dd > 1).sum())}\t{qb}\t{qa}\t{cstr}\n")
            f.flush()
            if diff.any():
                ys, xs = np.nonzero(diff)
                montage(img, ref, diff, (xs.min(), ys.min(), xs.max(), ys.max()), a.img / f"{s}.{c}.png")
    f.close()


if __name__ == "__main__":
    main()
