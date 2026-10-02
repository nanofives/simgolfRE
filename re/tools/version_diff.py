"""Match functions between two builds (v1.00 -> v1.03) from Ghidra fingerprints and report what changed.

    py -3.12 re/tools/version_diff.py re/versions/v100.jsonl re/versions/v103.jsonl

Matching passes, most certain first (each pass only pairs functions still unmatched on both sides):
  1 exact     identical normalized instructions (addresses masked), unique on both sides; then
              duplicate groups of equal size paired in address order (exact-ordered)
  2 strings   identical non-empty set of referenced strings, unique on both sides
  3 callgraph for an already-matched pair, their unmatched callees with equal shape, or the single
              unmatched callee left on each side (iterated to a fixed point)
  4 shape     identical mnemonic sequence, unique on both sides
  5 similar   best difflib ratio over mnemonic sequences (>= 0.75, size within 30%), mutual best
Each pair is then classified: identical (exact equal), operands (same shape, different operands:
constants/registers), changed (different instruction sequence). Unpaired = removed / added.

Writes re/versions/v100_v103.tsv and re/versions/v100_v103.md (changed functions with the constants and
strings that appeared or disappeared, the evidence that links a change to the patch notes).
"""
from __future__ import annotations

import argparse
import collections
import difflib
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]


def load(path):
    fns = {}
    for line in open(path, encoding="utf-8"):
        f = json.loads(line)
        f["mnl"] = f["mn"].split(";")[:-1]
        fns[f["entry"]] = f
    return fns


def unique_index(fns, unmatched, key):
    idx = collections.defaultdict(list)
    for e in unmatched:
        k = key(fns[e])
        if k:
            idx[k].append(e)
    return {k: v[0] for k, v in idx.items() if len(v) == 1}


def match(a: dict, b: dict):
    pairs: dict[str, tuple[str, str]] = {}  # a_entry -> (b_entry, pass)
    ua, ub = set(a), set(b)

    def take(x, y, how):
        pairs[x] = (y, how)
        ua.discard(x)
        ub.discard(y)

    def unique_pass(key, how):
        ia, ib = unique_index(a, ua, key), unique_index(b, ub, key)
        for k, x in ia.items():
            if k in ib:
                take(x, ib[k], how)

    unique_pass(lambda f: f["exact"] if f["nins"] > 2 else None, "exact")

    def ordered_pass(key, how):
        """Duplicate keys: when a group has the same size on both sides, pair in address order
        (the linker keeps object-file order between builds of the same source)."""
        ga, gb = collections.defaultdict(list), collections.defaultdict(list)
        for e in ua:
            ga[key(a[e])].append(e)
        for e in ub:
            gb[key(b[e])].append(e)
        for k, xs in ga.items():
            ys = gb.get(k, [])
            if k and len(xs) == len(ys) > 1:
                for x, y in zip(sorted(xs), sorted(ys)):
                    take(x, y, how)

    ordered_pass(lambda f: f["exact"], "exact-ordered")
    unique_pass(lambda f: "\x1f".join(f["strings"]) if f["strings"] else None, "strings")

    changed = True
    while changed:  # call-graph propagation to a fixed point
        changed = False
        for x, (y, _) in list(pairs.items()):
            ca = [c for c in a[x]["calls"] if c in ua]
            cb = [c for c in b[y]["calls"] if c in ub]
            if len(ca) == 1 and len(cb) == 1:
                take(ca[0], cb[0], "callgraph")
                changed = True
                continue
            sb = collections.defaultdict(list)
            for c in cb:
                sb[b[c]["shape"]].append(c)
            sa = collections.defaultdict(list)
            for c in ca:
                sa[a[c]["shape"]].append(c)
            for s, xs in sa.items():
                if len(xs) == 1 and len(sb.get(s, [])) == 1:
                    take(xs[0], sb[s][0], "callgraph")
                    changed = True

    unique_pass(lambda f: f["shape"] if f["nins"] > 2 else None, "shape")
    ordered_pass(lambda f: f["shape"], "shape-ordered")

    # similarity: mutual best among size-compatible candidates
    def best(src, dst, fs, fd):
        out = {}
        for x in src:
            n = fs[x]["nins"]
            if n < 4:
                continue
            cands = [y for y in dst if 0.7 * n <= fd[y]["nins"] <= 1.3 * n]
            scored = [(difflib.SequenceMatcher(None, fs[x]["mnl"], fd[y]["mnl"], autojunk=False).ratio(), y) for y in cands]
            if scored:
                r, y = max(scored)
                if r >= 0.75:
                    out[x] = (y, r)
        return out

    ba, bb = best(ua, ub, a, b), best(ub, ua, b, a)
    for x, (y, r) in ba.items():
        if bb.get(y, (None,))[0] == x:
            take(x, y, f"similar:{r:.2f}")
    return pairs, ua, ub


def classify(fa, fb):
    if fa["exact"] == fb["exact"]:
        return "identical"
    if fa["shape"] == fb["shape"]:
        return "operands"
    return "changed"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("old")
    ap.add_argument("new")
    ap.add_argument("--out", default=str(ROOT / "re" / "versions" / "v100_v103"))
    a = ap.parse_args()
    A, B = load(a.old), load(a.new)
    pairs, ua, ub = match(A, B)
    rows = []
    for x, (y, how) in pairs.items():
        fa, fb = A[x], B[y]
        kind = classify(fa, fb)
        ca, cb = set(fa["consts"]), set(fb["consts"])
        sa, sb = set(fa["strings"]), set(fb["strings"])
        rows.append({"old": x, "new": y, "how": how, "kind": kind, "nins_old": fa["nins"], "nins_new": fb["nins"],
                     "consts_added": sorted(cb - ca, key=lambda v: int(v, 16)), "consts_removed": sorted(ca - cb, key=lambda v: int(v, 16)),
                     "strings_added": sorted(sb - sa), "strings_removed": sorted(sa - sb), "strings": sorted(sb)})
    stats = collections.Counter(r["kind"] for r in rows)
    hows = collections.Counter(r["how"].split(":")[0] for r in rows)
    out = pathlib.Path(a.out)
    with open(out.with_suffix(".tsv"), "w", encoding="utf-8") as f:
        f.write("old\tnew\thow\tkind\tnins_old\tnins_new\tconsts_added\tconsts_removed\tstrings_added\tstrings_removed\n")
        for r in sorted(rows, key=lambda r: r["new"]):
            f.write("\t".join([r["old"], r["new"], r["how"], r["kind"], str(r["nins_old"]), str(r["nins_new"]),
                               ",".join(r["consts_added"]), ",".join(r["consts_removed"]),
                               " | ".join(r["strings_added"]), " | ".join(r["strings_removed"])]) + "\n")
        for x in sorted(ua):
            f.write(f"{x}\t\t\tremoved\t{A[x]['nins']}\t\t\t\t\t\n")
        for y in sorted(ub):
            f.write(f"\t{y}\t\tadded\t\t{B[y]['nins']}\t\t\t\t\n")
    md = [f"# Version diff {pathlib.Path(a.old).stem} -> {pathlib.Path(a.new).stem}", "",
          f"Functions: old {len(A)}, new {len(B)}. Matched {len(pairs)} "
          f"({', '.join(f'{k} {v}' for k, v in hows.most_common())}).",
          f"Classified: {', '.join(f'{k} {v}' for k, v in stats.most_common())}; removed {len(ua)}, added {len(ub)}.", "",
          "## Changed functions (instruction sequence differs), largest delta first", "",
          "| v1.00 | v1.03 | matched by | ins old->new | consts added | consts removed | strings (new) |",
          "|---|---|---|---|---|---|---|"]
    ch = sorted((r for r in rows if r["kind"] == "changed"), key=lambda r: -abs(r["nins_new"] - r["nins_old"]))
    for r in ch:
        md.append(f"| {r['old']} | {r['new']} | {r['how']} | {r['nins_old']}->{r['nins_new']} | "
                  f"{' '.join(r['consts_added'][:8])} | {' '.join(r['consts_removed'][:8])} | "
                  f"{'; '.join(s.replace('|', '/') for s in r['strings'][:3])} |")
    md += ["", "## Operand-only changes (same instructions, different constants/registers)", "",
           "| v1.00 | v1.03 | consts added | consts removed |", "|---|---|---|---|"]
    for r in rows:
        if r["kind"] == "operands" and (r["consts_added"] or r["consts_removed"]):
            md.append(f"| {r['old']} | {r['new']} | {' '.join(r['consts_added'])} | {' '.join(r['consts_removed'])} |")
    md += ["", f"## Added in v1.03 ({len(ub)})", "", ", ".join(sorted(ub)), "", f"## Removed from v1.00 ({len(ua)})", "", ", ".join(sorted(ua))]
    out.with_suffix(".md").write_text("\n".join(md) + "\n", encoding="utf-8")
    print(md[2]); print(md[3]); print(f"-> {out.with_suffix('.md')}")


if __name__ == "__main__":
    main()
