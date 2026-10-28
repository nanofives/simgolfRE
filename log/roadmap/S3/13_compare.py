"""S3.13: the gate outputs of the S3_13 mirror (log/roadmap/S3/13_*) against I9's (log/roadmap/merges/I9/) and, for the suites, the
I7b baseline (re/roadmap/BASELINE.md: log/roadmap/merges/I7/suite overridden by I7/b/suite). Adapted from I9's suite_compare.py,
boot_compare.py, segs_compare.py, ledger_compare.py and census_px.py.

    ~/bin/py -3.12 log/roadmap/S3/13_compare.py suite x64|win32
    ~/bin/py -3.12 log/roadmap/S3/13_compare.py boot x64|win32|ft_x64
    ~/bin/py -3.12 log/roadmap/S3/13_compare.py segs
    ~/bin/py -3.12 log/roadmap/S3/13_compare.py ledger
    ~/bin/py -3.12 log/roadmap/S3/13_compare.py census
(run from the shared root)"""
import collections
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
I7 = ROOT / "log/roadmap/merges/I7"
I9 = ROOT / "log/roadmap/merges/I9"
S = ROOT / "log/roadmap/S3"


def rows(p):
    lines = [l for l in open(p, encoding="utf-8") if not l.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t"))


def keyed(p, *k):
    return {tuple(r[x] for x in k) if len(k) > 1 else r[k[0]]: r for r in rows(p)}


# ------------------------------------------------------------------------------------------------ suite
def counts(p):
    last = [l for l in p.read_text(errors="replace").splitlines() if l.strip()]
    last = last[-1] if last else ""
    c = dict((k, int(v)) for v, k in re.findall(r"(\d+) (passed|failed|skipped|errors?|xfailed|xpassed|deselected)", last))
    return c, last


def load(dirs):
    out = {}
    for d in dirs:
        if d.is_dir():
            for f in d.glob("test_*.txt"):
                if f.name.endswith(".rerun.txt") or f.stem == "test_live" or f.stem.startswith("test_app_"):
                    continue
                out[f.stem] = counts(f)
    return out


def suite(be):
    sub = "suite" if be == "x64" else "suite_win32"
    base = load([I7 / sub, I7 / "b" / sub])
    i9 = load([I9 / sub])
    new = load([S / f"13_{sub}"])
    n = collections.Counter()
    for k in sorted(set(base) | set(i9) | set(new)):
        nb, ni, nn = base.get(k), i9.get(k), new.get(k)
        vs_i9 = "-" if nn is None else ("NEW" if ni is None else ("SAME" if ni[0] == nn[0] else "CHANGED"))
        vs_b = "-" if nn is None else ("NEW" if nb is None else ("SAME" if nb[0] == nn[0] else "CHANGED"))
        failed = bool(nn and (nn[0].get("failed") or nn[0].get("error") or nn[0].get("errors")))
        n["files"] += nn is not None
        n["fail_files"] += failed
        n["vsI9_" + vs_i9] += 1
        n["vsI7b_" + vs_b] += 1
        print(f"{'FAIL' if failed else 'ok':4} {k:34} vsI9 {vs_i9:7} vsI7b {vs_b:7} S3.13: {(nn or ({}, '-'))[1][:80]}")
        if vs_i9 == "CHANGED":
            print(f"{'':44}I9: {ni[1][:80]}")
        if failed and nb is not None:
            print(f"{'':44}I7b: {nb[1][:80]}")
    print("summary", dict(n))


# ------------------------------------------------------------------------------------------------ boot
def boot(be):
    head = keyed(ROOT / "log/p5/boot_gate_loc.tsv", "scenario")
    i9 = keyed(I9 / f"boot_{be}.tsv", "scenario")
    if be == "win32":   # I9's cov_a_keys3 re-run alone (its full run lost the boot to a low commit charge)
        for s, r in keyed(I9 / "boot_win32_keys3_rerun.tsv", "scenario").items():
            i9[s] = r
    new = keyed(S / f"13_boot_{be}.tsv", "scenario")
    cols = ("status", "records", "expected", "detail")
    diff = [s for s in sorted(set(i9) | set(new)) if any((i9.get(s) or {}).get(c) != (new.get(s) or {}).get(c) for c in cols)]
    allc = collections.Counter(r["status"] for r in new.values())
    hc = collections.Counter(r["status"] for s, r in new.items() if s in head)
    print(f"{be}: {len(new)} rows {dict(allc)}; HEAD's {sum(1 for s in new if s in head)}: {dict(hc)}; rows differing from I9: {len(diff)}")
    for s in diff:
        print("  ", s, {c: ((i9.get(s) or {}).get(c), (new.get(s) or {}).get(c)) for c in cols})


# ------------------------------------------------------------------------------------------------ segments
def segs():
    cols = ("status", "frames_run", "frames_expected", "digest")

    def cmp(a, b):
        return [n for n in sorted(set(a) | set(b)) if any((a.get(n) or {}).get(c) != (b.get(n) or {}).get(c) for c in cols)]

    new = {}
    for be in ("x64", "win32", "ft_x64"):
        p = S / f"13_segs_{be}.tsv"
        if not p.exists():
            continue
        new[be] = keyed(p, "name")
        for tag, base in (("I9", keyed(I9 / f"segs_{be}.tsv", "name")), ("I7b", keyed(I7 / "b" / f"segs_{be}.tsv", "name"))):
            d = cmp(base, new[be])
            print(f"{be}: {len(new[be])} rows {dict(collections.Counter(r['status'] for r in new[be].values()))}; "
                  f"rows differing from {tag}: {len(d)}")
            for n in d[:20]:
                print("  ", n, {c: ((base.get(n) or {}).get(c), (new[be].get(n) or {}).get(c)) for c in cols})
    if "x64" in new and "win32" in new:
        print(f"x64 vs win32: rows differing {len(cmp(new['x64'], new['win32']))}")


# ------------------------------------------------------------------------------------------------ defects / ledger / chain
def ledger():
    timing = {"seconds", "wall_s", "s", "time"}
    drop = lambda r: {c: v for c, v in r.items() if c not in timing}
    a, b = keyed(I9 / "defects_x64.tsv", "switch"), keyed(S / "13_defects_x64.tsv", "switch")
    diff = [s for s in sorted(set(a) | set(b)) if s not in a or s not in b or drop(a[s]) != drop(b[s])]
    print(f"defects: I9 {len(a)} switches, S3.13 {len(b)}, rows differing {len(diff)}")
    for s in diff:
        print("  ", s, "I9:", a.get(s) and drop(a[s]), "\n      S3.13:", b.get(s) and drop(b[s]))
    lines = lambda p: [l for l in open(p, encoding="utf-8").read().splitlines() if l.strip() and not l.startswith("#")]
    la, lb = lines(I9 / "ledger_divergences.tsv"), lines(S / "13_ledger_divergences.tsv")
    print(f"ledger: I9 {len(la)} lines, S3.13 {len(lb)}, only in I9 {len(set(la) - set(lb))}, only in S3.13 {len(set(lb) - set(la))}")
    for l in sorted(set(la) - set(lb)):
        print("   I9 only:", l[:300])
    for l in sorted(set(lb) - set(la)):
        print("   S3.13 only:", l[:300])
    noc = [r for r in rows(S / "13_ledger_divergences.tsv") if r.get("explained") == "NO"]
    print(f"ledger rows without a cause: {len(noc)}", [(r['kind'], r['name']) for r in noc])
    cols = ("status", "frames_run", "frames_expected", "record", "digest")
    ca, cb = keyed(I9 / "chain_x64.tsv", "chain", "checkpoint"), keyed(S / "13_chain_x64.tsv", "chain", "checkpoint")
    cd = [n for n in sorted(set(ca) | set(cb)) if any((ca.get(n) or {}).get(c) != (cb.get(n) or {}).get(c) for c in cols)]
    print(f"chain: I9 {len(ca)} rows, S3.13 {len(cb)} {dict(collections.Counter(r['status'] for r in cb.values()))}, "
          f"rows differing {len(cd)}", cd[:10])


# ------------------------------------------------------------------------------------------------ census
def census():
    old, new = keyed(I9 / "census.tsv", "scenario", "checkpoint"), keyed(S / "13_census.tsv", "scenario", "checkpoint")
    print("S3.13", len(new), "rows", dict(collections.Counter(r["status"] for r in new.values())),
          "| I9", len(old), dict(collections.Counter(r["status"] for r in old.values())))
    lost = [k for k, o in old.items() if o["status"] == "PASS" and (new.get(k) or {}).get("status") != "PASS"]
    gained = [k for k, n in new.items() if n["status"] == "PASS" and (old.get(k) or {}).get("status") != "PASS"]
    print("PASS lost vs I9:", len(lost), lost[:20])
    print("PASS gained vs I9:", len(gained), gained[:40])
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
    print("per row vs I9:", dict(cnt))
    for d, k, a, b in sorted(worse, reverse=True)[:40]:
        print(f"  worse +{d:7d} {k[0]}/{k[1]}: {a} -> {b}")
    for d, k, a, b in sorted(better)[:40]:
        print(f"  better {d:8d} {k[0]}/{k[1]}: {a} -> {b}")


if __name__ == "__main__":
    what = sys.argv[1]
    {"suite": lambda: suite(sys.argv[2]), "boot": lambda: boot(sys.argv[2]), "segs": segs, "ledger": ledger,
     "census": census}[what]()
