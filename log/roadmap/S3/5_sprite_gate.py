"""S3.5 sprite-pixel gate: per in-game census checkpoint, the pixels of the course view whose final value a scene-family draw gave
must equal the original's back buffer exactly.

The pixels are found without touching the original: one framegate boot of the scenario as usual (run A) and one with the scene
family's TEST-ONLY mask mode (g_negctl_gfx_scene = 7: every pixel a scene body writes is stored inverted, run B). A pixel that
differs between A and B got its final value from a scene draw (or from a later draw that read it: planes, halving, blends).
Per checkpoint of kind `ingame`:
  sprite_px     pixels of the view region in that mask
  sprite_bad    of those, pixels where A differs from the reference (must be 0)
  other_beyond  view pixels outside the mask differing beyond one level (other layers: text, lines, cursor, tooltips; or a
                sprite the port did not draw at all)
  other_one     view pixels outside the mask differing by one level (the terrain rasteriser, judged by G2 in S3.2's rule)
plus the row's status and the scene-related unsupported reasons (`drawQueuedSprite`, `Sprite_draw`, `Surface_drawSprite`,
`Image_draw`, `addSprite`) before the snapshot.

    cd port/build/S3_5_root && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/5_sprite_gate.py --all --jobs 2 \
        --out ../../../log/roadmap/S3/5_sprite_gate.tsv
    options: scenario names instead of --all; --negctl N (a control mode of the scene family for both runs, run A's mode)
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import json
import os
import pathlib
import shutil
import sys
import tempfile
import time

import numpy as np

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "re" / "tools"))
from port.test import framegate as FG  # noqa: E402

COLS = ["scenario", "checkpoint", "status", "kind", "terrain_drawn", "sprite_px", "sprite_bad", "sprite_bad_beyond", "other_beyond", "other_one",
        "scene_unsupported", "unsupported_before", "seconds"]
SCENE_WORDS = ("drawQueuedSprite", "Sprite_draw", "Surface_drawSprite", "Image_draw", "addSprite", "composeGolferSprite")


def load_png(p: pathlib.Path):
    from PIL import Image
    return np.asarray(Image.open(p).convert("RGB")) if p.exists() else None


def one_scenario(scenario: str, negctl: int, keep: str | None) -> list[dict]:
    from port.test import lib as L
    lib = L.load()
    tmp = pathlib.Path(tempfile.mkdtemp(prefix=f"s35_{scenario}_"))
    t0 = time.time()
    try:
        rows_a = FG.run_scenario(scenario, images=tmp / "A", probe=False,
                                 negctl=("gfx.family.scene", negctl) if negctl else None)
        if not any(r.get("kind") == "ingame" for r in rows_a):
            return []
        FG.run_scenario(scenario, images=tmp / "B", probe=False, negctl=("gfx.family.scene", 7))
        lab, names = FG.label_map("ingame")
        view = lab == names.index("view")
        out = []
        for r in rows_a:
            if r.get("kind") != "ingame":
                continue
            c = r["checkpoint"]
            a = load_png(tmp / "A" / f"{scenario}.{c}.port.png")
            b = load_png(tmp / "B" / f"{scenario}.{c}.port.png")
            row = {k: r.get(k, "") for k in ("scenario", "checkpoint", "status", "kind", "terrain_drawn", "unsupported_before")}
            row["seconds"] = round(time.time() - t0, 1)
            row["scene_unsupported"] = "|".join(u for u in (r.get("unsupported_before") or "").split("|")
                                                if u and any(w in u for w in SCENE_WORDS))
            if a is None or b is None:
                row.update(sprite_px=-1, sprite_bad=-1, other_beyond=-1, other_one=-1)
                out.append(row)
                continue
            ref = FG.GX.reference(scenario, c)
            mask = (a != b).any(axis=2) & view
            d5 = np.abs((a.astype(np.int16) >> 3) - (ref.astype(np.int16) >> 3)).max(axis=2)
            diff = (a != ref).any(axis=2)
            row.update(sprite_px=int(mask.sum()), sprite_bad=int((diff & mask).sum()), sprite_bad_beyond=int((mask & (d5 > 1)).sum()),
                       other_beyond=int(((d5 > 1) & view & ~mask).sum()), other_one=int(((d5 == 1) & view & ~mask).sum()))
            if keep and row["sprite_bad"]:
                from PIL import Image
                kd = pathlib.Path(keep)
                kd.mkdir(parents=True, exist_ok=True)
                Image.fromarray(a).save(kd / f"{scenario}.{c}.port.png")
                Image.fromarray(((diff & mask) * 255).astype(np.uint8)).save(kd / f"{scenario}.{c}.spritebad.png")
                Image.fromarray((mask * 255).astype(np.uint8)).save(kd / f"{scenario}.{c}.mask.png")
            out.append(row)
        return out
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# ------------------------------------------------------------------------------------------------ --queue: the exact oracle
QUEUE_COLS = ["scenario", "checkpoint", "ord", "sprite_px", "sprite_bad", "sprite_bad_beyond", "other_beyond", "other_one", "status"]
SCENE_DIR = ROOT.parents[2] / "port" / "corpus" / "scene" if (ROOT.name == "S3_5_root") else ROOT / "port" / "corpus" / "scene"


def boot_queue(scenario: str, ords: list[int], negctl: int, max_records: int, to_end: bool):
    """one boot with gfx and LiveTerrain attached; the root surface at each renderSpriteQueue return in `ords`"""
    import ctypes as C
    from port.test import boot as B
    from port.test import lib as L
    from port.test import terrainlive as TLm
    lib = L.load()
    out = {}
    va = FG.terrain_address(scenario)
    if getattr(lib, "dll", None) is not None:
        FG._bind_live(lib.dll)

    def before(st):
        st.call("gfx.attach", [])
        nm = st.map(0x30000000, bytearray(0x1000))
        raw = b"gfx_scene.captureQueueAt\0"
        C.memmove(C.addressof(nm), raw, len(raw))
        for o in ords:
            st.call("gfx.call", [0x30000000, o] + [0] * 10)
        if TLm.live_attach(lib, st, va) != 0:
            raise RuntimeError("sg_ts_live_attach failed")

    def after(st, _res):
        for o in ords:
            raw = st.files_read(f"$gfx/queue_{o}.555")
            out[o] = None if raw is None else np.frombuffer(raw, "<u2").reshape(FG.H, FG.W).copy()
        st.call("gfx.detach", [])
        TLm.live_detach(lib, st)

    prev = lib.negctl("gfx.family.scene", negctl) if negctl else None
    try:
        res = B.run(scenario, lib, max_records=max_records, before=before, after=after, to_end=to_end)
    finally:
        if negctl:
            lib.negctl("gfx.family.scene", prev or 0)
    return out, res


def one_scenario_queue(scenario: str, negctl: int, keep: str | None = None) -> list[dict]:
    import zlib
    from port.test import boot as B
    import trace_format as TF
    d = SCENE_DIR / scenario
    idx = json.loads((d / "index.json").read_text())
    cps = {c: m for c, m in idx["checkpoints"].items() if not m.get("none") and (d / f"{c}.z").exists()}
    if not cps:
        return []
    # records: through the frame after the last checkpoint's (framegate.run_scenario's budget)
    tr = TF.read(B.TRACES / f"{scenario}.trace")
    tgt, k = {}, 0
    for ordinal, _ra, _slot, kind, value in TF.clock_calls(tr):
        if kind == 1:
            tgt[k] = ordinal
            k += 1
    summ = json.loads((FG.GX.REFS / scenario / "det_b" / "summary.json").read_text())
    last = max(tgt[summ["checkpoints"][c]["status"]["tgt"]] for c in cps)
    frames_pos = [r.f["pos"] for r in tr.records if r.type == TF.FRAME]
    max_records = sum(1 for p in frames_pos if p <= last) + 2
    to_end = max_records > len(frames_pos)
    del tr
    ords = sorted({m["ord"] for m in cps.values()})
    a, res = boot_queue(scenario, ords, negctl, max_records, to_end)
    b, _ = boot_queue(scenario, ords, 7, max_records, to_end)
    lab, names = FG.label_map("ingame")
    view = lab == names.index("view")
    rows = []
    for c, m in sorted(cps.items()):
        o = m["ord"]
        orig = np.frombuffer(zlib.decompress((d / f"{c}.z").read_bytes()), "<u2").reshape(FG.H, FG.W)
        pa, pb = a.get(o), b.get(o)
        row = {"scenario": scenario, "checkpoint": c, "ord": o, "status": res.status}
        if pa is None or pb is None:
            row.update(sprite_px=-1, sprite_bad=-1, other_beyond=-1, other_one=-1)
            rows.append(row)
            continue
        mask = (pa != pb) & view
        diff = pa != orig
        ch = lambda v, s: ((v >> s) & 0x1f).astype(np.int16)
        d5 = np.maximum.reduce([np.abs(ch(pa, s) - ch(orig, s)) for s in (0, 5, 10)])
        row.update(sprite_px=int(mask.sum()), sprite_bad=int((diff & mask).sum()), sprite_bad_beyond=int((mask & (d5 > 1)).sum()),
                   other_beyond=int(((d5 > 1) & view & ~mask).sum()), other_one=int(((d5 == 1) & view & ~mask).sum()))
        if keep and row["sprite_bad_beyond"]:
            from PIL import Image
            kd = pathlib.Path(keep)
            kd.mkdir(parents=True, exist_ok=True)
            rgb = lambda v: np.stack([((v >> s) & 0x1f).astype(np.uint8) << 3 for s in (10, 5, 0)], axis=2)
            Image.fromarray(rgb(orig)).save(kd / f"{scenario}.{c}.orig.png")
            Image.fromarray(rgb(pa)).save(kd / f"{scenario}.{c}.port.png")
            Image.fromarray(((mask & (d5 > 1)) * 255).astype(np.uint8)).save(kd / f"{scenario}.{c}.bad.png")
        rows.append(row)
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scenarios", nargs="*")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--negctl", type=int, default=0)
    ap.add_argument("--out", required=True)
    ap.add_argument("--keep", help="directory for the images of checkpoints with sprite_bad > 0")
    ap.add_argument("--queue", action="store_true", help="the exact oracle: the ORIGINAL's root at renderSpriteQueue's return "
                                                         "(port/corpus/scene, 5_tools/scene_rec.py) against the port's")
    a = ap.parse_args()
    if a.queue:
        scen = sorted(p.name for p in SCENE_DIR.iterdir() if (p / "index.json").exists()) if a.all else a.scenarios
        rows = []
        with cf.ProcessPoolExecutor(max_workers=max(1, min(a.jobs, 2))) as ex:
            futs = {ex.submit(one_scenario_queue, s, a.negctl, a.keep): s for s in scen}
            for i, f in enumerate(cf.as_completed(futs), 1):
                s = futs[f]
                try:
                    rs = f.result()
                except Exception as e:  # noqa: BLE001
                    rs = [{"scenario": s, "checkpoint": "-", "status": "ERROR " + repr(e)[:200]}]
                rows.extend(rs)
                print(f"[{i}/{len(scen)}] {s} rows={len(rs)} sprite_bad_rows={sum(1 for r in rs if (r.get('sprite_bad') or 0) > 0)}",
                      flush=True)
        with open(a.out, "w", encoding="utf-8") as fh:
            fh.write(chr(9).join(QUEUE_COLS) + chr(10))
            for r in sorted(rows, key=lambda r: (r["scenario"], str(r["checkpoint"]))):
                fh.write(chr(9).join(str(r.get(k, "")) for k in QUEUE_COLS) + chr(10))
        tot = {"checkpoints": len(rows), "sprite_px": sum(max(0, r.get("sprite_px") or 0) for r in rows),
               "sprite_bad_px": sum(max(0, r.get("sprite_bad") or 0) for r in rows),
               "sprite_bad_rows": sum(1 for r in rows if (r.get("sprite_bad") or 0) > 0)}
        print(json.dumps(tot))
        pathlib.Path(a.out).with_suffix(".json").write_text(json.dumps(tot, indent=1))
        return
    scen = FG.all_scenarios() if a.all else a.scenarios
    out = pathlib.Path(a.out)
    rows: list[dict] = []
    done = 0
    with cf.ProcessPoolExecutor(max_workers=max(1, min(a.jobs, 2))) as ex:
        futs = {ex.submit(one_scenario, s, a.negctl, a.keep): s for s in scen}
        for f in cf.as_completed(futs):
            s = futs[f]
            done += 1
            try:
                rs = f.result()
            except Exception as e:  # noqa: BLE001
                rs = [{"scenario": s, "checkpoint": "-", "status": "ERROR", "unsupported_before": repr(e)[:300]}]
            rows.extend(rs)
            bad = sum(1 for r in rs if (r.get("sprite_bad") or 0) > 0)
            print(f"[{done}/{len(scen)}] {s} rows={len(rs)} sprite_bad_rows={bad}", flush=True)
            with open(out, "w", encoding="utf-8") as fh:
                fh.write("\t".join(COLS) + "\n")
                for r in sorted(rows, key=lambda r: (r["scenario"], str(r.get("checkpoint")))):
                    fh.write("\t".join(str(r.get(k, "")) for k in COLS) + "\n")
    tot = {"checkpoints": len(rows), "sprite_bad_rows": sum(1 for r in rows if (r.get("sprite_bad") or 0) > 0),
           "sprite_px": sum(max(0, r.get("sprite_px") or 0) for r in rows),
           "sprite_bad_px": sum(max(0, r.get("sprite_bad") or 0) for r in rows),
           "scene_unsupported_rows": sum(1 for r in rows if r.get("scene_unsupported"))}
    print(json.dumps(tot))
    out.with_suffix(".json").write_text(json.dumps(tot, indent=1))


if __name__ == "__main__":
    main()
