"""Three-axis scoreboard: Reach x Understanding x Verification, per function and for the project.

    py -3.12 scripts/scoreboard.py            # writes re/scoreboard.tsv, prints summary + top priorities
    py -3.12 scripts/scoreboard.py --top 40

Axes (0..3 each), all computed from evidence on disk; nothing is hand-entered:

  Reach (R)          from re/coverage/*.tsv (INT3 coverage census per scenario phase)
                     0 never seen, 1 one phase, 2 two phases, 3 three or more phases.
                     "?" when no coverage file exists yet (treated as 0 for priority).
  Understanding (U)  from hooks.csv confidence: C0=0, C1=1, C2=2, C3/C4=3.
  Verification (V)   strongest GREEN evidence in log/diff/ (or a reccmp match report):
                     0 none
                     1 path1    hand-picked A/B vectors (diff_hook.py)
                     2 replay   >= REPLAY_MIN recorded real calls replayed in Unicorn (replay.py),
                                or a symbolic proof (proof)
                     3 match    100% instruction match with the original compiler (re/tools/match.py, VC6)
                       scenario canonical-scenario ON/OFF diff with the hook live

Priority = R * ((3 - U) + (3 - V)): reachable code that is neither understood nor verified first.
Dead code (R = 0) never outranks live code, whatever its other axes say (the Terrain::tileAt lesson).
"""
from __future__ import annotations

import argparse
import csv
import pathlib
from collections import Counter

ROOT = pathlib.Path(__file__).resolve().parents[1]
REPLAY_MIN = 100
C_TO_U = {"C0": 0, "C1": 1, "C2": 2, "C3": 3, "C4": 3}


def read_tsv(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def verdict(path: pathlib.Path) -> tuple[bool, int]:
    rows = list(csv.reader(path.open(newline="")))
    v = next((r for r in rows if r and r[0] == "VERDICT"), None)
    n = sum(1 for r in rows[1:] if r and r[0] not in ("VERDICT", "install_witness", "meta"))
    return bool(v and v[1] == "GREEN"), n


def verification(root: pathlib.Path) -> dict[str, tuple[int, str]]:
    """addr -> (level, evidence file). Highest GREEN level wins."""
    out: dict[str, tuple[int, str]] = {}
    d = root / "log" / "diff"
    if not d.exists():
        return out
    for f in d.glob("*.csv"):
        addr, kind = f.name.split("_", 1)[0], f.name.rsplit(".", 2)[-2]
        green, n = verdict(f)
        if not green:
            continue
        level = {"path1": 1, "replay": 2 if n >= REPLAY_MIN else 1, "match": 3, "proof": 2, "scenario": 3}.get(kind, 0)
        if level > out.get(addr, (0, ""))[0]:
            out[addr] = (level, f.relative_to(root).as_posix())
    return out


MODULE_TSVS = {"golf_clean.exe": "functions_ghidra.tsv", "Terrain.dll": "functions_ghidra_Terrain.dll.tsv"}


def reach(root: pathlib.Path) -> tuple[dict[tuple[str, str], set[str]], bool]:
    """(module, addr) -> {"scenario:phase"}. Coverage files without a module column are golf_clean.exe."""
    phases: dict[tuple[str, str], set[str]] = {}
    files = sorted((root / "re" / "coverage").glob("*.tsv")) if (root / "re" / "coverage").exists() else []
    for f in files:
        scen = f.stem
        for r in read_tsv(f):
            key = (r.get("module") or "golf_clean.exe", r["addr"].lower())
            for p in filter(None, r["phases"].split(",")):
                phases.setdefault(key, set()).add(f"{scen}:{p}")
    return phases, bool(files)


def build(root: pathlib.Path = ROOT) -> list[dict]:
    hooks = {}
    if (root / "hooks.csv").exists():
        for r in csv.DictReader(open(root / "hooks.csv", newline="")):
            hooks[(r.get("module") or "golf_clean.exe", r["addr"])] = r
    ver = verification(root)
    ph, have_cov = reach(root)
    rows = []
    for mod, tsv in MODULE_TSVS.items():
        if not (root / "re" / tsv).exists():
            continue
        for f in read_tsv(root / "re" / tsv):
            a = f["entry"].lower()
            seen = ph.get((mod, a), set())
            r = min(len(seen), 3) if have_cov else None
            h = hooks.get((mod, a), {})
            u = C_TO_U.get(h.get("confidence") or "C0", 0)
            v, ev = ver.get(a, (0, ""))
            rows.append({
                "module": mod, "addr": a, "name": h.get("name") or f["name"], "size": int(f["size"]),
                "reach": "?" if r is None else r, "understanding": u, "verification": v,
                "priority": (r or 0) * ((3 - u) + (3 - v)),
                "phases": ";".join(sorted(seen)), "evidence": ev,
            })
    rows.sort(key=lambda x: (-x["priority"], -x["size"]))
    return rows


def summary(rows: list[dict]) -> str:
    n = len(rows)
    live = [r for r in rows if r["reach"] not in ("?", 0)]
    c = lambda key, rs: Counter(r[key] for r in rs)  # noqa: E731
    mods = Counter(r["module"] for r in rows)
    lines = [f"functions: {n} ({', '.join(f'{m} {c}' for m, c in mods.items())})   reached: {len(live) if rows and rows[0]['reach'] != '?' else '?'}"]
    for axis in ("reach", "understanding", "verification"):
        lines.append(f"  {axis:13} " + "  ".join(f"{k}={v}" for k, v in sorted(c(axis, rows).items(), key=lambda kv: str(kv[0]))))
    if live:
        und = sum(1 for r in live if r["understanding"] >= 2)
        ver = sum(1 for r in live if r["verification"] >= 2)
        lines.append(f"  live code understood (U>=2): {und}/{len(live)} ({100 * und / len(live):.1f}%)   "
                     f"verified (V>=2): {ver}/{len(live)} ({100 * ver / len(live):.1f}%)")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--top", type=int, default=20)
    ap.add_argument("--root", type=pathlib.Path, default=ROOT)
    a = ap.parse_args()
    rows = build(a.root)
    out = a.root / "re" / "scoreboard.tsv"
    with out.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]), delimiter="\t")
        w.writeheader()
        w.writerows(rows)
    print(summary(rows))
    print(f"\ntop {a.top} priorities (R*((3-U)+(3-V))):")
    for r in rows[:a.top]:
        print(f"  {r['module'][:7]:7} {r['addr']}  P={r['priority']:2}  R={r['reach']} U={r['understanding']} V={r['verification']}  "
              f"{r['size']:5}b  {r['name']}  [{r['phases']}]")
    print(f"-> {out.relative_to(a.root)}")


if __name__ == "__main__":
    main()
