"""S3.12: the wave 6 tree's gate outputs (log/roadmap/S3/12_out/S3_12/) against 01d84188's (I13: log/roadmap/merges/I13/, the
mirror whose content 01d84188 committed) and, for the terrain verdicts, S3.2's 2_terrain_verdicts.tsv.

    ~/bin/py -3.12 log/roadmap/S3/12_compare.py segs | boot | census | terrain | s7 | suite x64|win32
(run from the shared root)"""
import collections
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
I13 = ROOT / "log/roadmap/merges/I13"
I10 = ROOT / "log/roadmap/merges/I10"
S = ROOT / "log/roadmap/S3"
N = S / "12_out" / "S3_12"
TIMING = {"seconds", "wall_s", "s", "time", "sec"}


def rows(p):
    lines = [l for l in open(p, encoding="utf-8") if not l.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t"))


def keyed(p, *k):
    return {tuple(r[x] for x in k) if len(k) > 1 else r[k[0]]: r for r in rows(p)}


def diff_rows(a, b, cols):
    return [n for n in sorted(set(a) | set(b)) if any((a.get(n) or {}).get(c) != (b.get(n) or {}).get(c) for c in cols)]


def segs():
    cols = ("status", "frames_run", "frames_expected", "digest")
    new = {}
    for be in ("x64", "win32"):
        p = N / f"segs_{be}.tsv"
        if not p.exists():
            print(be, "missing")
            continue
        new[be] = keyed(p, "name")
        base = keyed(I13 / f"segs_{be}.tsv", "name")
        d = diff_rows(base, new[be], cols)
        print(f"segs {be}: {len(new[be])} rows {dict(collections.Counter(r['status'] for r in new[be].values()))}; "
              f"rows differing from e179bd24 (I13 tables; I13 port/ = e179bd24 port/): {len(d)}", d[:10])
    if len(new) == 2:
        print("x64 vs win32 rows differing:", len(diff_rows(new["x64"], new["win32"], cols)))


def boot():
    cols = ("status", "records", "expected", "detail")
    for be in ("x64", "win32"):
        p = N / f"boot_{be}.tsv"
        if not p.exists():
            print(be, "missing")
            continue
        new, base = keyed(p, "scenario"), keyed(I13 / f"boot_{be}.tsv", "scenario")
        d = diff_rows(base, new, cols)
        print(f"boot {be}: {len(new)} rows {dict(collections.Counter(r['status'] for r in new.values()))}; rows differing from "
              f"e179bd24 (I13 tables; I13 port/ = e179bd24 port/): {len(d)}")
        for s in d:
            print("   ", s, {c: ((base.get(s) or {}).get(c), (new.get(s) or {}).get(c)) for c in cols})


def census():
    new, old = keyed(N / "census.tsv", "scenario", "checkpoint"), keyed(I13 / "census.tsv", "scenario", "checkpoint")
    print("S3_12", len(new), "rows", dict(collections.Counter(r["status"] for r in new.values())))
    print("I13  ", len(old), "rows", dict(collections.Counter(r["status"] for r in old.values())))
    lost = [k for k, o in old.items() if o["status"] == "PASS" and (new.get(k) or {}).get("status") != "PASS"]
    gained = [k for k, n in new.items() if n["status"] == "PASS" and (old.get(k) or {}).get("status") != "PASS"]
    print("PASS lost:", len(lost), lost[:20], " PASS gained:", len(gained), gained[:20])
    cnt, worse = collections.Counter(), []
    for k, n in new.items():
        o = old.get(k)
        try:
            a, b = int(o["total_px"]), int(n["total_px"])
        except (TypeError, ValueError, KeyError):
            cnt["unjudged"] += 1
            continue
        c = "better" if b < a else "same" if b == a else "worse"
        cnt[c] += 1
        if c == "worse":
            worse.append((b - a, k))
    print("per row total_px vs I13:", dict(cnt), sorted(worse, reverse=True)[:10])


def terrain():
    b = rows(N / "terrain_verdicts.tsv")
    k = list(b[0].keys())[:2]
    B = {tuple(r[c] for c in k): r for r in b}
    print("S3_12 terrain:", dict(collections.Counter(r.get("verdict", r.get("status")) for r in b)))
    for tag, p in (("e179bd24 (I13 tables; I13 port/ = e179bd24 port/)", I13 / "terrain_verdicts.tsv"), ("S3.2", S / "2_terrain_verdicts.tsv")):
        a = rows(p)
        A = {tuple(r[c] for c in k): r for r in a}
        common = [c for c in b[0].keys() if c in a[0].keys() and c not in TIMING]
        d = [x for x in sorted(set(A) & set(B)) if any(A[x].get(c) != B[x].get(c) for c in common)]
        print(f"  vs {tag}: {len(A)} rows there, common keys {len(set(A) & set(B))}, rows differing in {common}: {len(d)}", d[:8])


def s7():
    for name, key in (("equiv_x64", "scenario"), ("app_replay_x64", None), ("app_replay_win32", None)):
        p, q = N / f"{name}.tsv", I13 / f"{name}.tsv"
        if not p.exists():
            print(name, "missing")
            continue
        a, b = rows(q), rows(p)
        k = key or list(b[0].keys())[0]
        drop = lambda r: {c: v for c, v in r.items() if c not in TIMING}
        A, B = {r[k]: drop(r) for r in a}, {r[k]: drop(r) for r in b}
        d = [x for x in sorted(set(A) | set(B)) if A.get(x) != B.get(x)]
        print(f"{name}: {len(B)} rows {dict(collections.Counter(r.get('status') for r in b))}; rows differing from I13: {len(d)}", d[:10])


def counts(p):
    last = [l for l in p.read_text(errors="replace").splitlines() if l.strip() and not l.startswith("restored ")]
    last = last[-1] if last else ""
    c = dict((k, int(v)) for v, k in re.findall(r"(\d+) (passed|failed|skipped|errors?|xfailed|xpassed|deselected)", last))
    c.pop("deselected", None)
    return c, last


def suite(be):
    sub = "suite" if be == "x64" else "suite_win32"
    new = {f.stem: counts(f) for f in (N / sub).glob("test_*.txt")}
    head = {f.stem: counts(f) for d in ([I13 / "suite", I13 / "suite_app"] if be == "x64" else [I13 / "suite_win32"])
            if d.is_dir() for f in d.glob("test_*.txt")}
    n = collections.Counter()
    for k in sorted(new):
        nn, nh = new[k], head.get(k)
        failed = bool(nn[0].get("failed") or nn[0].get("error") or nn[0].get("errors"))
        vs = "NEW" if nh is None else ("SAME" if nh[0] == nn[0] else "CHANGED")
        n["files"] += 1
        n["fail_files"] += failed
        n["vsI13_" + vs] += 1
        print(f"{'FAIL' if failed else 'ok':4} {k:34} vsI13 {vs:7} {nn[1][:90]}")
        if vs == "CHANGED":
            print(f"{'':45}I13: {nh[1][:90]}")
    print("summary", dict(n))
    if be == "win32":
        known = {l.strip() for l in open(I10 / "win32_failed_ids.txt") if l.strip()}
        got = set()
        for f in (N / sub).glob("test_*.txt"):
            got |= {l.strip() for l in f.read_text(errors="replace").splitlines() if l.startswith("FAILED ")}
        got = {g.split(" - ")[0] for g in got}
        print(f"win32 failed ids: {len(got)}; known {len(known)}; new (not in I10's list): {sorted(got - known)}; "
              f"known that passed: {len(known - got)}")


if __name__ == "__main__":
    w = sys.argv[1]
    {"segs": segs, "boot": boot, "census": census, "terrain": terrain, "s7": s7, "suite": lambda: suite(sys.argv[2])}[w]()
