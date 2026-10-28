"""I13: the gate outputs of the mirror port/build/I13 (log/roadmap/merges/I13/) against HEAD d66aa755's (I10, log/roadmap/merges/I10/)
and S3.13's tree on 78ffb06d (log/roadmap/S3/13_*). Adapted from log/roadmap/S3/13_compare.py.

    ~/bin/py -3.12 log/roadmap/merges/I13/compare.py suite x64|win32
    ~/bin/py -3.12 log/roadmap/merges/I13/compare.py boot x64|win32|ft_x64
    ~/bin/py -3.12 log/roadmap/merges/I13/compare.py segs
    ~/bin/py -3.12 log/roadmap/merges/I13/compare.py ledger
    ~/bin/py -3.12 log/roadmap/merges/I13/compare.py census
    ~/bin/py -3.12 log/roadmap/merges/I13/compare.py s7            (live equivalence, app replay vs I10)
    ~/bin/py -3.12 log/roadmap/merges/I13/compare.py terrain
(run from the shared root)"""
import collections
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
I10 = ROOT / "log/roadmap/merges/I10"
I7 = ROOT / "log/roadmap/merges/I7"
I9 = ROOT / "log/roadmap/merges/I9"
S = ROOT / "log/roadmap/S3"
N = ROOT / "log/roadmap/merges/I13"


def rows(p):
    lines = [l for l in open(p, encoding="utf-8") if not l.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t"))


def keyed(p, *k):
    return {tuple(r[x] for x in k) if len(k) > 1 else r[k[0]]: r for r in rows(p)}


def counts(p):
    last = [l for l in p.read_text(errors="replace").splitlines() if l.strip()]
    last = last[-1] if last else ""
    c = dict((k, int(v)) for v, k in re.findall(r"(\d+) (passed|failed|skipped|errors?|xfailed|xpassed|deselected)", last))
    c.pop("deselected", None)
    return c, last


def load(dirs):
    out = {}
    for d in dirs:
        if d.is_dir():
            for f in d.glob("test_*.txt"):
                if f.name.endswith(".rerun.txt"):
                    continue
                out[f.stem] = counts(f)
    return out


def suite(be):
    sub = "suite" if be == "x64" else "suite_win32"
    head = load([I10 / sub] + ([I10 / "suite_applive2"] if be == "x64" else []))
    s3 = load([S / f"13_{sub}"])
    new = load([N / sub] + ([N / "suite_app"] if be == "x64" else []))
    n = collections.Counter()
    for k in sorted(set(head) | set(s3) | set(new)):
        nh, ns, nn = head.get(k), s3.get(k), new.get(k)
        vs_h = "-" if nn is None else ("NEW" if nh is None else ("SAME" if nh[0] == nn[0] else "CHANGED"))
        vs_s = "-" if nn is None else ("NEW" if ns is None else ("SAME" if ns[0] == nn[0] else "CHANGED"))
        failed = bool(nn and (nn[0].get("failed") or nn[0].get("error") or nn[0].get("errors")))
        n["files"] += nn is not None
        n["fail_files"] += failed
        n["vsHEAD_" + vs_h] += 1
        n["vsS3.13_" + vs_s] += 1
        print(f"{'FAIL' if failed else 'ok':4} {k:34} vsHEAD {vs_h:7} vsS3.13 {vs_s:7} I13: {(nn or ({}, '-'))[1][:80]}")
        if vs_h == "CHANGED":
            print(f"{'':46}HEAD: {nh[1][:80]}")
        if vs_s == "CHANGED":
            print(f"{'':46}S3.13: {ns[1][:80]}")
    print("summary", dict(n))


def boot(be):
    head_sc = keyed(ROOT / "log/p5/boot_gate_loc.tsv", "scenario")
    cols = ("status", "records", "expected", "detail")
    new = keyed(N / f"boot_{be}.tsv", "scenario")
    allc = collections.Counter(r["status"] for r in new.values())
    hc = collections.Counter(r["status"] for s, r in new.items() if s in head_sc)
    print(f"{be}: {len(new)} rows {dict(allc)}; the 61 original scenarios: {dict(hc)}")
    bases = [("S3.13", S / f"13_boot_{be}.tsv")]
    if be != "ft_x64":
        bases.insert(0, ("HEAD(I10)", I10 / f"boot_{be}.tsv"))
    for tag, p in bases:
        b = keyed(p, "scenario")
        diff = [s for s in sorted(set(b) | set(new)) if any((b.get(s) or {}).get(c) != (new.get(s) or {}).get(c) for c in cols)]
        print(f"  rows differing from {tag}: {len(diff)}")
        for s in diff:
            print("    ", s, {c: ((b.get(s) or {}).get(c), (new.get(s) or {}).get(c)) for c in cols})


def segs():
    cols = ("status", "frames_run", "frames_expected", "digest")

    def cmp(a, b):
        return [n for n in sorted(set(a) | set(b)) if any((a.get(n) or {}).get(c) != (b.get(n) or {}).get(c) for c in cols)]

    new = {}
    for be in ("x64", "win32", "ft_x64"):
        p = N / f"segs_{be}.tsv"
        if not p.exists():
            continue
        new[be] = keyed(p, "name")
        bases = [("S3.13", S / f"13_segs_{be}.tsv"), ("I7b", I7 / "b" / f"segs_{be}.tsv")]
        if be != "ft_x64":
            bases.insert(0, ("HEAD(I10)", I10 / f"segs_{be}.tsv"))
        print(f"{be}: {len(new[be])} rows {dict(collections.Counter(r['status'] for r in new[be].values()))}")
        for tag, bp in bases:
            if not bp.exists():
                print(f"  (no {tag} file {bp.name})")
                continue
            base = keyed(bp, "name")
            d = cmp(base, new[be])
            print(f"  rows differing from {tag}: {len(d)}")
            for n in d[:20]:
                print("    ", n, {c: ((base.get(n) or {}).get(c), (new[be].get(n) or {}).get(c)) for c in cols})
    for a, b in (("x64", "win32"), ("x64", "ft_x64")):
        if a in new and b in new:
            print(f"{a} vs {b}: rows differing {len(cmp(new[a], new[b]))}")


def ledger():
    timing = {"seconds", "wall_s", "s", "time"}
    drop = lambda r: {c: v for c, v in r.items() if c not in timing}
    b = keyed(N / "defects_x64.tsv", "switch")
    for tag, p in (("HEAD(I10)", I10 / "defects_x64.tsv"), ("S3.13", S / "13_defects_x64.tsv")):
        a = keyed(p, "switch")
        diff = [s for s in sorted(set(a) | set(b)) if s not in a or s not in b or drop(a[s]) != drop(b[s])]
        print(f"defects vs {tag}: {len(a)} switches there, {len(b)} here, rows differing {len(diff)}")
        for s in diff:
            print("  ", s, f"{tag}:", a.get(s) and drop(a[s]), "\n      I13:", b.get(s) and drop(b[s]))
    lines = lambda p: [l for l in open(p, encoding="utf-8").read().splitlines() if l.strip() and not l.startswith("#")]
    lb = lines(N / "ledger_divergences.tsv")
    for tag, p in (("HEAD(I10)", I10 / "ledger_divergences.tsv"), ("S3.13", S / "13_ledger_divergences.tsv")):
        la = lines(p)
        print(f"ledger vs {tag}: {len(la)} lines there, {len(lb)} here, only there {len(set(la) - set(lb))}, only here {len(set(lb) - set(la))}")
        for l in sorted(set(la) - set(lb)):
            print("   only there:", l[:300])
        for l in sorted(set(lb) - set(la)):
            print("   only here:", l[:300])
    noc = [r for r in rows(N / "ledger_divergences.tsv") if r.get("explained") == "NO"]
    print(f"ledger rows without a cause: {len(noc)}", [(r['kind'], r['name']) for r in noc])
    cols = ("status", "frames_run", "frames_expected", "record", "digest")
    cb = keyed(N / "chain_x64.tsv", "chain", "checkpoint")
    for tag, p in (("HEAD(I10)", I10 / "chain_x64.tsv"), ("S3.13", S / "13_chain_x64.tsv")):
        ca = keyed(p, "chain", "checkpoint")
        cd = [n for n in sorted(set(ca) | set(cb)) if any((ca.get(n) or {}).get(c) != (cb.get(n) or {}).get(c) for c in cols)]
        print(f"chain vs {tag}: {len(ca)} rows there, {len(cb)} here {dict(collections.Counter(r['status'] for r in cb.values()))}, "
              f"rows differing {len(cd)}", cd[:10])


def census():
    new = keyed(N / "census.tsv", "scenario", "checkpoint")
    print("I13", len(new), "rows", dict(collections.Counter(r["status"] for r in new.values())))
    for tag, p in (("HEAD(I10)", I10 / "census.tsv"), ("S3.13", S / "13_census.tsv")):
        old = keyed(p, "scenario", "checkpoint")
        print(f"== vs {tag}: {len(old)} rows", dict(collections.Counter(r["status"] for r in old.values())))
        lost = [k for k, o in old.items() if o["status"] == "PASS" and (new.get(k) or {}).get("status") != "PASS"]
        gained = [k for k, n in new.items() if n["status"] == "PASS" and (old.get(k) or {}).get("status") != "PASS"]
        print("PASS lost:", len(lost), lost[:40])
        print("PASS gained:", len(gained), gained[:40])
        trans = collections.Counter(((old.get(k) or {}).get("status"), n["status"]) for k, n in new.items()
                                    if (old.get(k) or {}).get("status") != n["status"])
        print("transitions:", dict(trans))
        cnt = collections.Counter()
        worse, better = [], []
        for k, n in new.items():
            o = old.get(k)
            try:
                a, b = int(o["total_px"]), int(n["total_px"])
            except (TypeError, ValueError, KeyError):
                cnt["unjudged"] += 1
                continue
            c = "better" if b < a else "same" if b == a else "worse"
            cnt[c] += 1
            (worse if c == "worse" else better if c == "better" else []).append((b - a, k, a, b))
        print("per row:", dict(cnt))
        for d, k, a, b in sorted(worse, reverse=True)[:40]:
            print(f"  worse +{d:7d} {k[0]}/{k[1]}: {a} -> {b}")


def s7():
    for name, key in (("equiv_x64", "scenario"), ("equiv_win32", "scenario"), ("app_replay_x64", None), ("app_replay_win32", None)):
        p, q = N / f"{name}.tsv", I10 / f"{name}.tsv"
        if not p.exists():
            print(name, "missing")
            continue
        a, b = rows(q), rows(p)
        k = key or list(b[0].keys())[0]
        drop = lambda r: {c: v for c, v in r.items() if c not in {"seconds", "wall_s", "s", "time"}}
        A, B = {r[k]: drop(r) for r in a}, {r[k]: drop(r) for r in b}
        d = [x for x in sorted(set(A) | set(B)) if A.get(x) != B.get(x)]
        print(f"{name}: {len(B)} rows {dict(collections.Counter(r.get('status') for r in b))}; rows differing from HEAD(I10): {len(d)}")
        for x in d[:20]:
            print("   ", x, A.get(x), "->", B.get(x))


def terrain():
    for tag, p in (("HEAD(I10)", I10 / "terrain_verdicts.tsv"), ("S3.13", S / "13_terrain_verdicts.tsv")):
        b = rows(N / "terrain_verdicts.tsv")
        if not p.exists():
            print("no", tag, p)
            continue
        a = rows(p)
        k = list(b[0].keys())[:2]
        A = {tuple(r[c] for c in k): r.get("verdict", r.get("status")) for r in a}
        B = {tuple(r[c] for c in k): r.get("verdict", r.get("status")) for r in b}
        d = [x for x in sorted(set(A) | set(B)) if A.get(x) != B.get(x)]
        print(f"terrain: {dict(collections.Counter(B.values()))}; vs {tag} {dict(collections.Counter(A.values()))}, verdicts differing {len(d)}",
              d[:10])


if __name__ == "__main__":
    what = sys.argv[1]
    {"suite": lambda: suite(sys.argv[2]), "boot": lambda: boot(sys.argv[2]), "segs": segs, "ledger": ledger,
     "census": census, "s7": s7, "terrain": terrain}[what]()
