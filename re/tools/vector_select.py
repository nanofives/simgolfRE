"""Phase 2.5 selection: which golf_clean.exe functions get a function-vector corpus (re/analysis/data/vector_corpus.md).

    py -3.12 re/tools/vector_select.py            # writes re/port/vector_selection.tsv, prints the counts

Tiers (first that applies):
  sim          C3/C4 hooks.csv rows with subsystem sim / golfer / economy / course / terrain / ai
  util_callee  C3/C4 util rows reachable from a `sim` row through hooks.csv callees (transitively, any subsystem between)
  crt_callee   C3/C4 crt rows reachable the same way (abs, itoa, memcpy ...: leaves the sim code calls)
  util_pure    the other C3/C4 util rows whose detour is `pure` or `globals` in log/port_hooks.tsv
               (port_survey_hooks.py: the leaves the port can take first)
If re/port/simcore_order.tsv (phase 2.0) exists, its rows are added as tier `simcore` when not already selected.
A row is selected when it has at least one registry key (re/frida/hooks_registry.py) from a batch linked into the shim
(shim/re_batches.txt, or the core registry). Capture-time exclusions (stand-ins, imports, heap, unmapped reads) are
decided by vector_capture.py from the run itself and written to re/port/vectors_manifest.tsv.
"""
from __future__ import annotations

import csv
import importlib.util
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "frida"))
OUT = ROOT / "re" / "port" / "vector_selection.tsv"
SIM = {"sim", "golfer", "economy", "course", "terrain", "ai"}


def key_batches() -> dict[str, str]:
    """registry key -> batch id (`core` for hooks_registry.py's own entries), as hooks_registry loads them."""
    import hooks_registry as HR
    out = {}
    src = (ROOT / "re" / "frida" / "hooks_registry.py").read_text()
    enabled = HR.enabled_batches()
    for f in sorted((ROOT / "re" / "frida" / "registry.d").glob("*.py"), key=lambda p: (p.stem not in enabled, p.stem)):
        spec = importlib.util.spec_from_file_location(f"vs_registry_{f.stem}", f)
        mod = importlib.util.module_from_spec(spec)
        try:
            spec.loader.exec_module(mod)
        except Exception:
            continue
        for k in mod.HOOKS:
            out.setdefault(k, f.stem)
    for k in HR.HOOKS:
        out.setdefault(k, "core")
    return out


def select():
    import hooks_registry as HR
    rows = {}
    for r in csv.DictReader((ROOT / "hooks.csv").open()):
        if r["module"] == "golf_clean.exe":
            rows[int(r["addr"], 16)] = r
    kind = {int(r["addr"], 16): r["kind"] for r in csv.DictReader((ROOT / "log" / "port_hooks.tsv").open(), delimiter="\t")}
    lvl = lambda a: rows[a]["confidence"] in ("C3", "C4")
    callees = lambda a: [int(x, 16) for x in rows[a]["callees"].split(";") if x.startswith("0x")]
    tier = {a: "sim" for a, r in rows.items() if r["subsystem"] in SIM and lvl(a)}
    seen, st = set(tier), list(tier)
    while st:
        a = st.pop()
        for c in callees(a):
            if c in rows and c not in seen:
                seen.add(c)
                st.append(c)
                if lvl(c) and rows[c]["subsystem"] == "util":
                    tier.setdefault(c, "util_callee")
                elif lvl(c) and rows[c]["subsystem"] == "crt":
                    tier.setdefault(c, "crt_callee")
    for a, r in rows.items():
        if lvl(a) and r["subsystem"] == "util" and kind.get(a) in ("pure", "globals"):
            tier.setdefault(a, "util_pure")
    order = ROOT / "re" / "port" / "simcore_order.tsv"
    simcore = set()
    if order.exists():
        for r in csv.DictReader(order.open(), delimiter="\t"):
            a = next((r[c] for c in ("addr", "va", "address") if c in r), None)
            if a:
                a = int(a, 16)
                simcore.add(a)
                if a in rows and lvl(a):
                    tier.setdefault(a, "simcore")
    kb = key_batches()
    enabled = HR.enabled_batches() | {"core"}
    keys = {}
    for k, v in HR.HOOKS.items():
        if (v.get("module") or "golf_clean.exe").lower() == "golf_clean.exe":
            keys.setdefault(v["addr"], []).append(k)
    out = []
    for a in sorted(tier):
        r = rows[a]
        ks = keys.get(a, [])
        live = [k for k in ks if kb.get(k) in enabled]
        if not ks:
            sel, why = 0, "no registry key"
        elif not live:
            sel, why = 0, "keys only in batches not linked into the shim"
        else:
            sel, why = 1, ""
        out.append({"addr": f"0x{a:08x}", "name": r["name"], "subsystem": r["subsystem"], "level": r["confidence"],
                    "tier": tier[a], "in_simcore": int(a in simcore), "port_kind": kind.get(a, "-"), "keys": len(live),
                    "batches": ",".join(sorted({kb[k] for k in live})) or "-", "key_list": " ".join(live) or "-",
                    "selected": sel, "reason": why or "-"})
    return out


def main():
    out = select()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(out[0]), delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(out)
    from collections import Counter
    print("in simcore_order.tsv:", sum(r["in_simcore"] for r in out))
    print("tiers:", dict(Counter(r["tier"] for r in out)))
    print("selected:", sum(r["selected"] for r in out), "of", len(out), "keys:", sum(r["keys"] for r in out if r["selected"]))
    print("not selected:", dict(Counter(r["reason"] for r in out if not r["selected"])))
    print(f"-> {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
