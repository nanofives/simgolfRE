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
        src = (JS_DIR / "diff_fixtures.js").read_text() + "\n" + (JS_DIR / "diff_hook.js").read_text()
        sc = g.session.create_script(src)
        sc.load()
        for name in names:
            spec = dict(HOOKS[name])
            spec["vectors"] = [list(v) for v in spec["vectors"]]
            call = {k: spec[k] for k in ("module", "addr", "abi", "ret", "args", "fixture", "vectors")}
            if override_re and name in override_re:
                call["override_re"] = override_re[name]
            r = sc.exports_sync.run(call)
            ok = r["witness"] == 0xE9 and all(row["orig"] == row["re"] for row in r["rows"])
            path = out_dir / f"{spec['addr']:08x}_{name}.path1.csv"
            with path.open("w", newline="") as f:
                w = csv.writer(f)
                w.writerow(["vector", "original", "reimpl", "match"])
                for row in r["rows"]:
                    w.writerow([" ".join(map(str, row["vector"])), row["orig"], row["re"], row["orig"] == row["re"]])
                w.writerow(["install_witness", f"0x{r['witness']:02x}", "0xe9", r["witness"] == 0xE9])
                w.writerow(["meta", f"detour={r['detour']}", f"original={r['original']}",
                            datetime.datetime.now().isoformat(timespec="seconds")])
                w.writerow(["VERDICT", "GREEN" if ok else "RED", len(r["rows"]), ""])
            mism = sum(row["orig"] != row["re"] for row in r["rows"])
            print(f"{name}: {'GREEN' if ok else 'RED'}  {len(r['rows'])} vectors, {mism} mismatches, "
                  f"witness 0x{r['witness']:02x} -> {path}")
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
