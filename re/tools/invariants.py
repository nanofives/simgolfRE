"""Daikon-style likely invariants from a replay bundle (recorded real calls), as a reusable oracle.

    py -3.12 re/tools/invariants.py TerrainDll_tileAt          # infer -> re/replay/<name>/invariants.json
    py -3.12 re/tools/invariants.py TerrainDll_tileAt --check other_calls.jsonl

Why: a recording of one function is also evidence about its NEIGHBOURS. Its arguments are what the
callers produce; its returns are what the callers consume. Inferred invariants become cheap checks:
when a caller is reimplemented, the arguments it passes must still satisfy the callee's invariants, and
a violation localizes the bug to the caller before any full-scenario diff.

Inferred (only properties that held on EVERY recorded call; NO-GUESSING: they are observations, not
semantics):
  per register/arg: constant, small value set (<= 8), range [min, max] (signed)
  return: NULL fraction, and "ret == NULL  <=>  arg_i outside [lo_i, hi_i]" when that split is exact
  pointer return: (ret - ecx) mod stride == base offset, with stride/base solved from the data
"""
from __future__ import annotations

import argparse
import json
import pathlib
import sys
from math import gcd

ROOT = pathlib.Path(__file__).resolve().parents[2]


def s32(v: int) -> int:
    return v - (1 << 32) if v >= 1 << 31 else v


def infer(calls: list[dict]) -> dict:
    inv: dict = {"calls": len(calls), "vars": {}, "ret": {}}
    cols = {"ecx": [c["ecx"] for c in calls]}
    nargs = len(calls[0]["args"]) if calls else 0
    for i in range(nargs):
        cols[f"arg{i}"] = [s32(c["args"][i]) for c in calls]
    for name, vals in cols.items():
        vs = sorted(set(vals))
        d = {"min": min(vals), "max": max(vals)}
        if len(vs) == 1:
            d["const"] = vs[0]
        elif len(vs) <= 8:
            d["one_of"] = vs
        inv["vars"][name] = d
    rets = [c["live"] for c in calls]
    nulls = [r == 0 for r in rets]
    inv["ret"]["null_fraction"] = sum(nulls) / len(rets)
    # NULL iff some arg leaves the range seen on non-NULL calls
    ok = [c for c, n in zip(calls, nulls) if not n]
    if ok and any(nulls):
        bounds = {f"arg{i}": (min(s32(c["args"][i]) for c in ok), max(s32(c["args"][i]) for c in ok)) for i in range(nargs)}

        def outside(c):
            return any(not (lo <= s32(c["args"][int(k[3:])]) <= hi) for k, (lo, hi) in bounds.items())
        if all(outside(c) == n for c, n in zip(calls, nulls)):
            inv["ret"]["null_iff_outside"] = {k: list(v) for k, v in bounds.items()}
    # pointer return relative to ecx: ret - ecx = base + stride * k
    offs = sorted({(c["live"] - c["ecx"]) & 0xFFFFFFFF for c in ok})
    if len(offs) >= 3:
        g = 0
        for o in offs[1:]:
            g = gcd(g, o - offs[0])
        if g > 1:
            inv["ret"]["ecx_relative"] = {"stride": g, "base": offs[0] % g,
                                          "min_offset": offs[0], "max_offset": offs[-1]}
    return inv


def check(inv: dict, calls: list[dict]) -> list[str]:
    bad = []
    for i, c in enumerate(calls):
        vals = {"ecx": c["ecx"], **{f"arg{j}": s32(v) for j, v in enumerate(c["args"])}}
        for k, d in inv["vars"].items():
            v = vals.get(k)
            if v is None:
                continue
            if "const" in d and v != d["const"] and k != "ecx":
                bad.append(f"call {i}: {k}={v} != const {d['const']}")
        r = inv["ret"]
        if "null_iff_outside" in r and "live" in c:
            out = any(not (lo <= vals[k] <= hi) for k, (lo, hi) in r["null_iff_outside"].items())
            if out != (c["live"] == 0):
                bad.append(f"call {i}: NULL={c['live'] == 0} but args {'outside' if out else 'inside'} {r['null_iff_outside']}")
        if "ecx_relative" in r and c.get("live"):
            off = (c["live"] - c["ecx"]) & 0xFFFFFFFF
            e = r["ecx_relative"]
            if off % e["stride"] != e["base"]:
                bad.append(f"call {i}: ret-ecx=0x{off:x} not = 0x{e['base']:x} mod 0x{e['stride']:x}")
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name")
    ap.add_argument("--check")
    a = ap.parse_args()
    d = ROOT / "re" / "replay" / a.name
    calls = [json.loads(l) for l in (d / "calls.jsonl").read_text().splitlines()]
    if a.check:
        inv = json.loads((d / "invariants.json").read_text())
        other = [json.loads(l) for l in open(a.check)]
        bad = check(inv, other)
        print(f"{len(other)} calls checked, {len(bad)} violations")
        for b in bad[:20]:
            print("  " + b)
        return 1 if bad else 0
    inv = infer(calls)
    (d / "invariants.json").write_text(json.dumps(inv, indent=2))
    print(json.dumps(inv, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
