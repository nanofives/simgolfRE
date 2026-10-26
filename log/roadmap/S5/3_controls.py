"""S5.3 gate (d): the raster's negative controls (g_negctl_fttext 5 glyph origin + 1 px, 6 colour one 5-bit red step, 7 the
raster's hinting off), each over the whole label corpus and the six checkpoints. A control is RED when its raster differs from
the unmodified FreeType raster (port/corpus/labels/rasters/freetype_<backend>.npz, `labels.py render freetype`); the table also
gives its pixel difference against GDI next to the unmodified raster's, and the jobs it leaves unchanged (blind subsets).

    SG_PORTSIM_DLL=<x64 dll> ~/bin/py -3.12 log/roadmap/S5/3_controls.py [--modes 5,6,7] [--backend x64|win32 --w32-dir ..]
"""
import argparse
import collections
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import labels as LB  # noqa: E402

WHAT = {5: "glyph origin + 1 px", 6: "colour: red one 5-bit step", 7: "raster hinting off"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--modes", default="5,6,7")
    ap.add_argument("--backend", default="x64", choices=["x64", "win32"])
    ap.add_argument("--w32-dir")
    ap.add_argument("--out", default=str(ROOT / "log" / "roadmap" / "S5" / "3_controls.json"))
    a = ap.parse_args()
    corpus = LB.load_corpus()
    ents = corpus["entries"]
    base = LB.load_rasters(LB.OUT / "rasters" / f"freetype_{a.backend}.npz")
    gdi = LB.load_rasters(LB.OUT / "rasters" / "gdi_x64.npz")
    lib = LB.load_lib(a.backend, a.w32_dir)
    image = LB.start_image(corpus["image"])
    base_vs_gdi = LB.compare_corpus(base, gdi, corpus)
    base_chk = LB.compare_checkpoints(lib, "freetype", image, a.backend)
    res = {"base_px_vs_gdi": base_vs_gdi["px_differ"],
           "base_checkpoints_px_vs_gdi": {k: v["px_vs_gdi"] for k, v in base_chk.items()}, "modes": {}}
    print(f"unmodified FreeType: {base_vs_gdi['px_differ']} px vs GDI over the corpus")
    for m in (int(x) for x in a.modes.split(",")):
        prev = lib.negctl("fttext.fontWidth", m)
        try:
            r = LB.render_corpus(lib, "freetype")
            chk = LB.compare_checkpoints(lib, "freetype", image, a.backend)
        finally:
            lib.negctl("fttext.fontWidth", prev or 0)
        changed = collections.Counter()
        blind = collections.Counter()
        for (cfg, eid, *_x), x, y in zip(corpus["jobs"], r, base):
            if (x != y).any():
                changed[f"0x{ents[eid]['fontref']:08x}"] += 1
            else:
                blind[cfg] += 1
        vs_gdi = LB.compare_corpus(r, gdi, corpus)
        chk_changed = {k: v["px_vs_gdi"] != base_chk[k]["px_vs_gdi"] for k, v in chk.items()}
        red = sum(changed.values()) > 0 and all(chk_changed.values())
        res["modes"][m] = {"what": WHAT.get(m, "?"), "jobs_changed": sum(changed.values()), "jobs": len(r),
                           "changed_per_font": dict(changed), "unchanged_per_config": dict(blind),
                           "px_vs_gdi": vs_gdi["px_differ"], "checkpoints_px_vs_gdi": {k: v["px_vs_gdi"] for k, v in chk.items()},
                           "checkpoints_changed": chk_changed, "red": red}
        print(f"mode {m} ({WHAT.get(m)}): {'RED' if red else 'NOT RED'}; corpus jobs changed {sum(changed.values())} / {len(r)}, "
              f"px vs GDI {vs_gdi['px_differ']} (unmodified {base_vs_gdi['px_differ']}); unchanged jobs per config "
              f"{dict(blind)}; checkpoints px vs GDI {res['modes'][m]['checkpoints_px_vs_gdi']} (unmodified "
              f"{res['base_checkpoints_px_vs_gdi']})", flush=True)
    pathlib.Path(a.out).write_text(json.dumps(res, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
