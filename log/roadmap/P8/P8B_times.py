"""P8B: pass-time table from the SG_TERRAIN_PASS_LOG (`view800`, the 800 x 600 passes) and SG_WORLD_PASS_LOG (`world`) files of
P8B_batch.sh's runs. Prints a TSV (run, layer, entry kind, passes, median / p90 / max / total ms) to stdout.
Entry kinds (terrain_render.h): 1 strip, 2 render, 3 local, 4 pathUpdate, 5 renderTileAtFiveOffsets; a world strip pass is drawn as
a full render (kind 1 in the world log means that)."""
import pathlib
import statistics as st

R = pathlib.Path(__file__).parent / "P8B_runs"


def load(p: pathlib.Path, kind_col: int, us_col: int):
    rows = []
    if not p.exists():
        return rows
    for line in p.read_text().splitlines():
        f = line.split()
        if len(f) > us_col:
            rows.append((int(f[kind_col]), int(f[us_col])))
    return rows


def main():
    print("run\tlayer\tkind\tpasses\tmedian_ms\tp90_ms\tmax_ms\ttotal_ms")
    runs = []
    for scen in ("cs", "cam"):
        runs.append(f"time_{scen}_off")
        runs += [f"time_{scen}_{s}_t{t}" for s in ("1067x600", "1920x1080", "2560x1440") for t in ("default", "1")]
    for d in runs:
        for name, rows in (("view800", load(R / f"{d}.view.log", 2, 3)), ("world", load(R / f"{d}.world.log", 1, 2))):
            if not rows:
                continue
            for k in sorted({r[0] for r in rows}) + ["all"]:
                us = sorted(r[1] for r in rows if k == "all" or r[0] == k)
                p90 = us[max(0, int(len(us) * 0.9) - 1)]
                print(f"{d}\t{name}\t{k}\t{len(us)}\t{st.median(us) / 1000:.2f}\t{p90 / 1000:.2f}\t{us[-1] / 1000:.2f}\t{sum(us) / 1000:.1f}")


if __name__ == "__main__":
    main()
