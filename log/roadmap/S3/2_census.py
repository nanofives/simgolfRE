"""S3.2 terrain census: for every census checkpoint whose terrain column is yes, the port's course view against the original's.

The original's course view at a checkpoint = its root surface at the return of the last updateTerrainRender before the checkpoint
(re/tools/terrain_calls.py --scenario <s> --pass-frames / --frames-lite: log/p5/terrain/<label>/frames/<checkpoint>/exit, with the
frame's entry). The port boots the same trace with gfx + LiveTerrain attached and copies its root surface at the same
updateTerrainRender ordinal (terrainLive.keepFrame). Verdict = G2 over the pixels the original's frame drew (changed from entry):
one-level <= 1.5 % of them, <= 70 further off (port/test/glraster.py), the rule S3.1 / S3.1b judge every pass by.

    cd port/build/S3_2_root && SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 ../../../log/roadmap/S3/2_census.py terrain --jobs 3 \
        --out ../../../log/roadmap/S3/2_terrain_verdicts.tsv
    ~/bin/py -3.12 log/roadmap/S3/2_census.py rank <framegate tsv of the S3.2 build> log/roadmap/S3/2_terrain_verdicts.tsv \
        --out log/roadmap/S3/2_census_ranked.tsv

`rank` re-ranks a framegate census like log/roadmap/S0/3_rank.py, with one change: the terrain cause of a terrain-yes row comes from
the terrain verdict of this tool (`terrain not drawn` when it fails or the port kept no course view, `terrain not measured` when no
recording holds the checkpoint), not from the view region of the final frame (whose sprites, HUD text and overlays are other steps'
causes); a row whose terrain verdict is PASS gets its 2D cause as the primary cause.
"""
import argparse
import collections
import concurrent.futures as cf
import csv
import json
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve()
ROOT = pathlib.Path.cwd() if (pathlib.Path.cwd() / "port" / "test").is_dir() else HERE.parents[3]
sys.path.insert(0, str(ROOT))
REPO = HERE.parents[3]
TERRAIN = REPO / "log" / "p5" / "terrain"
CENSUS = REPO / "re" / "roadmap" / "frame_census.tsv"
# full recordings first (they hold the same exit frames as the lite ones)
PREFER = ["s32_", "s32q_", "s32p_", "s32c_"]
COLS = ["scenario", "checkpoint", "label", "frame", "drawn", "diff", "one", "big", "verdict", "boot", "detail"]


def census_rows(path=None):
    lines = [ln for ln in pathlib.Path(path or CENSUS).read_text(encoding="utf-8").splitlines() if not ln.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t"))


ALIASES = {"sandbox_basic": ["s32_sandbox"]}   # the first recording's label


def label_for(scenario: str) -> str | None:
    for lab in ALIASES.get(scenario, []) + [p + scenario for p in PREFER]:
        d = TERRAIN / lab
        if (d / "frames").is_dir() and any((d / "frames").iterdir()):
            return lab
    return None


def worker(scenario: str, checkpoints: list[str]) -> list[dict]:
    from port.test import lib as L, terrainlive as TL
    label = label_for(scenario)
    base = {"scenario": scenario, "label": label or ""}
    if label is None:
        return [dict(base, checkpoint=c, verdict="NOT_MEASURED", detail="no recording") for c in checkpoints]
    rec = TL.recorded_frames(label)
    have = [c for c in checkpoints if c in rec]
    out = [dict(base, checkpoint=c, verdict="NOT_MEASURED", detail="the recording has no frame before this checkpoint")
           for c in checkpoints if c not in rec]
    if not have:
        return out
    lib = L.load()
    last = max(have, key=lambda c: rec[c]["meta"]["frame"])
    try:
        got = TL.port_frames(lib, scenario, sorted({rec[c]["meta"]["frame"] for c in have}), last)
    except Exception as e:  # noqa: BLE001
        return out + [dict(base, checkpoint=c, verdict="FAIL", detail=f"port run: {e!r}"[:200]) for c in have]
    for c in have:
        e = rec[c]
        k = e["meta"]["frame"]
        p = got.get((k, "exit"))
        row = dict(base, checkpoint=c, frame=k, boot=got["boot"].status)
        if p is None:
            row.update(verdict="FAIL", detail=f"the port kept no course view of frame {k} (boot {got['boot'].status})")
        else:
            r = TL.g2(e["exit"], p, e["entry"])
            row.update(drawn=r["drawn"], diff=r["diff"], one=r["one"], big=r["big"], verdict="PASS" if r["pass"] else "FAIL")
        out.append(row)
    return out


def cmd_terrain(a):
    rows = [r for r in census_rows(a.census) if r["terrain"] == "yes"]
    by = collections.defaultdict(list)
    for r in rows:
        by[r["scenario"]].append(r["checkpoint"])
    names = [s for s in by if not a.scenarios or s in a.scenarios]
    res = []

    def one(s):
        cmd = [sys.executable, str(HERE), "worker", s, ",".join(by[s])]
        p = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ROOT))
        ln = p.stdout.strip().splitlines()
        if p.returncode == 0 and ln and ln[-1].startswith("["):
            return json.loads(ln[-1])
        return [{"scenario": s, "checkpoint": c, "verdict": "FAIL", "detail": f"worker: {(p.stderr or p.stdout)[-200:]}"}
                for c in by[s]]

    with cf.ThreadPoolExecutor(a.jobs) as ex:
        futs = {ex.submit(one, s): s for s in names}
        for i, f in enumerate(cf.as_completed(futs), 1):
            rr = f.result()
            res += rr
            cnt = collections.Counter(r["verdict"] for r in rr)
            print(f"[{i}/{len(names)}] {futs[f]} {dict(cnt)}", flush=True)
    if a.merge and pathlib.Path(a.out).exists():   # keep the rows of the scenarios not re-run
        done = set(names)
        res += [r for r in csv.DictReader(open(a.out, encoding="utf-8"), delimiter="\t") if r["scenario"] not in done]
    order = {(r["scenario"], r["checkpoint"]): i for i, r in enumerate(rows)}
    res.sort(key=lambda r: order.get((r["scenario"], r["checkpoint"]), 1 << 30))
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        w = csv.DictWriter(f, COLS, delimiter="\t", lineterminator="\n", extrasaction="ignore")
        w.writeheader()
        w.writerows(res)
    print(json.dumps(dict(collections.Counter(r["verdict"] for r in res))))


def cmd_rank(a):
    sys.path.insert(0, str(REPO))
    spec_rows = list(csv.DictReader(open(a.census, encoding="utf-8"), delimiter="\t"))
    tv = {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(open(a.terrain, encoding="utf-8"), delimiter="\t")}
    out = []
    for r in spec_rows:
        t = tv.get((r["scenario"], r["checkpoint"]))
        r["terrain_verdict"] = (t or {}).get("verdict", "") if r.get("terrain") == "yes" else ""
        r["terrain_g2"] = f"{t.get('drawn')}/{t.get('one')}/{t.get('big')}" if t and t.get("drawn") else ""
        if r["status"] == "PASS":
            r["primary_cause"] = "PASS"
        elif r.get("terrain") == "yes" and r["terrain_verdict"] == "FAIL":
            r["primary_cause"] = "terrain not drawn"
        elif r.get("terrain") == "yes" and r["terrain_verdict"] in ("", "NOT_MEASURED"):
            r["primary_cause"] = "terrain not measured"
        else:
            if r["status"] == "NOT_RUN":
                r["primary_cause"] = "NOT_RUN: " + (r.get("detail") or r.get("boot_status", ""))
            elif r.get("first_unsupported_drawing"):
                r["primary_cause"] = "unsupported: " + r["first_unsupported_drawing"]
            else:
                r["primary_cause"] = "FAIL no unsupported call (" + r.get("kind", "") + ")"
        out.append(r)
    cols = list(spec_rows[0].keys()) + [c for c in ("terrain_verdict", "terrain_g2", "primary_cause") if c not in spec_rows[0]]
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        w = csv.DictWriter(f, cols, delimiter="\t", lineterminator="\n", extrasaction="ignore")
        w.writeheader()
        w.writerows(out)
    pc = collections.Counter(r["primary_cause"] for r in out)
    print("rows", len(out), "status", dict(collections.Counter(r["status"] for r in out)))
    print("terrain verdicts (terrain yes):", dict(collections.Counter(r["terrain_verdict"] for r in out if r.get("terrain") == "yes")))
    for k, v in pc.most_common(30):
        print(f"{v:5d}  {k}")


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    t = sub.add_parser("terrain")
    t.add_argument("scenarios", nargs="*")
    t.add_argument("--jobs", type=int, default=3)
    t.add_argument("--out", required=True)
    t.add_argument("--census", help="the census TSV whose terrain-yes rows are measured (default re/roadmap/frame_census.tsv)")
    t.add_argument("--merge", action="store_true", help="keep the out file's rows of the scenarios not given")
    w = sub.add_parser("worker")
    w.add_argument("scenario")
    w.add_argument("checkpoints")
    r = sub.add_parser("rank")
    r.add_argument("census")
    r.add_argument("terrain")
    r.add_argument("--out", required=True)
    a = ap.parse_args()
    if a.cmd == "worker":
        print(json.dumps(worker(a.scenario, a.checkpoints.split(","))))
    elif a.cmd == "terrain":
        cmd_terrain(a)
    else:
        cmd_rank(a)


if __name__ == "__main__":
    main()
