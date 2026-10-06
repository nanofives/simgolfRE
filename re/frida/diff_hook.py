"""Path-1 A/B diff for one registered hook: original (trampoline) vs reimplementation, same inputs,
inside the live game. Writes log/diff/<addr>_<name>.path1.csv ending in a VERDICT row.

    py -3.12 re/frida/diff_hook.py Terrain_tileAt
    py -3.12 re/frida/diff_hook.py --all

Evidence grade: path1 is C3-grade at most (proves the body is bit-identical on these vectors, not that
the game behaves the same with the hook live). C4 needs a canonical-scenario run (see re/CONFIDENCE.md).
"""
from __future__ import annotations

import argparse
import csv
import datetime
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(pathlib.Path(__file__).parent))

from game import Game, JS_DIR  # noqa: E402
from hooks_registry import HOOKS  # noqa: E402

DIFF_DIR = ROOT / "log" / "diff"


def run(names: list[str], override_re: dict[str, int | str] | None = None, out_dir: pathlib.Path = DIFF_DIR) -> dict[str, bool]:
    """override_re (tests only): an address, or "null" for a stub returning NULL/0, called instead of
    the detour to prove a wrong body reads RED."""
    if Game.running_instances():
        raise SystemExit("SimGolf already running; the game is single-instance")
    if override_re and out_dir == DIFF_DIR:
        raise ValueError("negative-control runs must not write into log/diff (evidence)")
    out_dir.mkdir(parents=True, exist_ok=True)
    results = {}
    with Game(env={"SIMGOLF_SKIP_INTRO": "1"}) as g:
        g.wait_window()
        # Fixtures and the runner must share one script runtime, so they load as one source.
        # base fixtures, then one fragment per C3 batch (js/fixtures.d/*.js: Object.assign(globalThis.DIFF_FIXTURES, {...}))
        # only batches listed in shim/re_batches.txt: a fragment still being written must not break the run
        from hooks_registry import enabled_batches
        frags = [p for p in sorted((JS_DIR / "fixtures.d").glob("*.js")) if p.stem in enabled_batches()]
        parts = [JS_DIR / "diff_fixtures.js"] + frags + [JS_DIR / "diff_hook.js"]
        src = "\n".join(p.read_text() for p in parts)
        sc = g.session.create_script(src)
        sc.load()
        for name in names:
            spec = dict(HOOKS[name])
            spec["vectors"] = [list(v) for v in spec["vectors"]]
            spec["state"] = [list(s) for s in spec.get("state", [])]
            call = {k: spec.get(k) for k in ("module", "addr", "abi", "ret", "args", "fixture", "vectors", "state")}
            if override_re and name in override_re:
                call["override_re"] = override_re[name]
            r = sc.exports_sync.run(call)
            same = lambda row: row["orig"] == row["re"] and row.get("state_match", True)
            ok = r["witness"] == 0xE9 and all(same(row) for row in r["rows"])
            stateful = bool(spec["state"])
            path = out_dir / f"{spec['addr']:08x}_{name}.path1.csv"
            with path.open("w", newline="") as f:
                w = csv.writer(f)
                w.writerow(["vector", "original", "reimpl", "match"] + (["state_original", "state_reimpl", "state_changed"] if stateful else []))
                for row in r["rows"]:
                    extra = [row["state_orig"], row["state_re"], row["state_changed"]] if stateful else []
                    w.writerow([" ".join(map(str, row["vector"])), row["orig"], row["re"], same(row)] + extra)
                if stateful:
                    w.writerow(["state_regions", " ".join(f"{b}+0x{o:x}:{n}" if isinstance(b, str) else f"0x{b:08x}+0x{o:x}:{n}"
                                                          for b, o, n in spec["state"]), "", ""])
                w.writerow(["install_witness", f"0x{r['witness']:02x}", "0xe9", r["witness"] == 0xE9])
                w.writerow(["meta", f"detour={r['detour']}", f"original={r['original']}",
                            datetime.datetime.now().isoformat(timespec="seconds")])
                distinct = len({row["orig"] for row in r["rows"]} | {row.get("state_orig") for row in r["rows"]} - {None})
                w.writerow(["distinct_results", distinct, "", ""])
                w.writerow(["VERDICT", "GREEN" if ok else "RED", len(r["rows"]), ""])
            mism = sum(not same(row) for row in r["rows"])
            st = (f", state checked ({sum(row['state_changed'] for row in r['rows'])} vectors change it, "
                  f"{sum(not row['state_match'] for row in r['rows'])} state mismatches)") if stateful else ""
            print(f"{name}: {'GREEN' if ok else 'RED'}  {len(r['rows'])} vectors, {mism} mismatches{st}, "
                  f"witness 0x{r['witness']:02x} -> {path}")
            changed = any(row.get("state_changed") for row in r["rows"])
            if len({row["orig"] for row in r["rows"]}) == 1 and len(r["rows"]) > 1 and not changed:
                print(f"  WARNING {name}: every vector returned {r['rows'][0]['orig']} and no state changed: the vectors "
                      "or the fixture do not exercise the function (a GREEN here is not evidence)")
            results[name] = ok
    return results


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("names", nargs="*")
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    names = list(HOOKS) if a.all else a.names
    unknown = [n for n in names if n not in HOOKS]
    if not names or unknown:
        raise SystemExit(f"unknown/none: {unknown or names}. Registered: {', '.join(HOOKS)}")
    res = run(names)
    return 0 if all(res.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
