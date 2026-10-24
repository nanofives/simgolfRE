"""S0.3: frame_census.tsv (census + terrain_verdict) and frame_causes.tsv from log/roadmap/S0/3_census.tsv.

Run in the HEAD export root (port/build/S0_head):  ~/bin/py -3.12 log/roadmap/S0/3_rank.py [census.tsv] [out_dir]

terrain_verdict (decision G2, 2026-10-24): for a terrain-yes row, on the `view` region of the in-game kind (the whole frame for a
terrain-yes row of another kind): one-level px / region AREA <= glraster.MAX_ONE_LEVEL and beyond-one-level px <= glraster.MAX_BIG.
The terrain pass draws no pixels at HEAD, so the denominator is the region's area, not a drawn-pixel count.
"""
import collections
import csv
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import framegate as FG  # noqa: E402
from port.test import glraster as GR   # noqa: E402

src = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "log/roadmap/S0/3_census.tsv")
outdir = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else ROOT / "log/roadmap/S0/3_out")
outdir.mkdir(parents=True, exist_ok=True)

rows = list(csv.DictReader(open(src, encoding="utf-8"), delimiter="\t"))
# scenarios re-run by 3_rerun_noend.py (boot -11 at the end of input with a snapshot pending, U-1120): their rows replace the census's
RERUN = {}
for p in sorted(src.parent.glob("3_rerun_*.json")):
    jr = json.loads(p.read_text().strip().splitlines()[-1])
    RERUN[jr[0]["scenario"]] = [{c: ("" if r.get(c) is None else str(r.get(c))) for c in rows[0].keys()} for r in jr]
if RERUN:
    rows = [r for r in rows if r["scenario"] not in RERUN]
    for s in RERUN:
        rows += RERUN[s]
    order = {}
    for r in csv.DictReader(open(src, encoding="utf-8"), delimiter="\t"):
        order.setdefault(r["scenario"], len(order))
    rows.sort(key=lambda r: order[r["scenario"]])
lab, names = FG.label_map("ingame")
VIEW_AREA = int((lab == names.index("view")).sum())
FRAME_AREA = FG.W * FG.H


def parse_regions(s):
    out = {}
    for item in filter(None, (s or "").split(";")):
        n, v = item.split("=")
        px, one, pct, beyond = v.split("/")
        out[n] = (int(px), int(one), float(pct), int(beyond))
    return out


def verdict(r):
    if r["status"] == "NOT_RUN" or r["terrain"] != "yes":
        return "", "", ""
    reg = parse_regions(r["regions"])
    if "view" in reg:
        px, one, _, beyond = reg["view"]
        area, where = VIEW_AREA, "view"
    else:
        px, one, beyond = int(r["total_px"]), int(r["one_level_px"]), int(r["beyond_px"])
        area, where = FRAME_AREA, "frame"
    frac = one / area
    ok = frac <= GR.MAX_ONE_LEVEL and beyond <= GR.MAX_BIG
    return ("PASS" if ok else "FAIL"), f"{where}:{px}/{one}/{beyond}", f"{100 * frac:.3f}"


def twod_cause(r):
    """2D cause of a non-PASS row: the first unsupported drawing call, else the regions with differing px (terrain-yes: view excluded)."""
    if r["status"] == "NOT_RUN":
        return "NOT_RUN: " + (r["detail"] or r["boot_status"])
    if r["first_unsupported_drawing"]:
        return "unsupported: " + r["first_unsupported_drawing"]
    reg = parse_regions(r["regions"])
    bad = [n for n, v in reg.items() if v[0] > 0 and not (n == "view" and r["terrain"] == "yes")]
    if not bad:
        return "2D regions clean (only view differs)" if r["terrain"] == "yes" else "FAIL: no region differs"
    return f"FAIL no unsupported call ({r['kind']}): " + ",".join(bad)


out_cols = list(rows[0].keys()) + ["terrain_verdict", "terrain_view_px_one_beyond", "terrain_one_pct_of_area", "twod_cause", "primary_cause"]
cens = []
for r in rows:
    tv, raw, pct = verdict(r)
    r["terrain_verdict"], r["terrain_view_px_one_beyond"], r["terrain_one_pct_of_area"] = tv, raw, pct
    if r["status"] == "PASS":
        r["twod_cause"], r["primary_cause"] = "", "PASS"
    else:
        r["twod_cause"] = twod_cause(r)
        if r["terrain"] == "yes" and tv == "FAIL":
            r["primary_cause"] = "terrain not drawn"
        else:
            r["primary_cause"] = r["twod_cause"]
    cens.append(r)

with open(outdir / "frame_census.tsv", "w", encoding="utf-8", newline="") as f:
    f.write("# terrain_verdict (G2): view region (area %d px; whole frame %d px for another kind) one-level px / area <= %s and beyond-one-level px <= %d (port/test/glraster.py MAX_ONE_LEVEL, MAX_BIG); denominator = region area because the terrain pass draws no pixels at HEAD; 2D regions keep the 0 px rule\n" % (VIEW_AREA, FRAME_AREA, GR.MAX_ONE_LEVEL, GR.MAX_BIG))
    w = csv.DictWriter(f, out_cols, delimiter="\t", lineterminator="\n", extrasaction="ignore")
    w.writeheader()
    w.writerows(cens)


def rank(key, subset, label):
    g = collections.defaultdict(list)
    for r in subset:
        g[r[key]].append(r)
    res = []
    for cause, rs in g.items():
        res.append((label, cause, len(rs), len({r["scenario"] for r in rs}),
                    sum(r["terrain"] == "yes" for r in rs), sum(r["terrain"] == "no" for r in rs),
                    f"{rs[0]['scenario']}/{rs[0]['checkpoint']}"))
    res.sort(key=lambda t: -t[2])
    return res


blocked = [r for r in cens if r["status"] != "PASS"]
lines = rank("primary_cause", blocked, "primary") + rank("twod_cause", blocked, "2d_all") + \
    rank("twod_cause", [r for r in blocked if r["terrain"] != "yes"], "2d_terrain_no") + \
    rank("twod_cause", [r for r in blocked if r["terrain"] == "yes"], "2d_terrain_yes")
with open(outdir / "frame_causes.tsv", "w", encoding="utf-8", newline="") as f:
    f.write("ranking\tcause\tcheckpoints_blocked\tscenarios\tterrain_yes\tterrain_no\texample_checkpoint\n")
    for t in lines:
        f.write("\t".join(map(str, t)) + "\n")

st = collections.Counter(r["status"] for r in cens)
tv = collections.Counter(r["terrain_verdict"] for r in cens if r["terrain"] == "yes")
print("rows", len(cens), "scenarios", len({r["scenario"] for r in cens}), dict(st), "terrain verdict", dict(tv))
print("primary total", sum(t[2] for t in lines if t[0] == "primary"), "blocked", len(blocked))
for t in lines[:25]:
    print(t)
