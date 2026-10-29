"""S4.10: census of the exe's backend-audio path-1 keys replayed without the game (S4.1's tool with golf_clean.exe as the
image): the fixture VM rebuilds each key's fixture, the ORIGINAL exe function runs in Unicorn twice per vector (arms 1 and
2), in layouts A / B / C (other fixture / emulator heaps and gaps; the exe is not relocatable, its base stays 0x400000), and
each vector is compared with the live path-1 CSV on the address-independent fields (S4.1's compare()).

    ~/bin/py -3.12 log/roadmap/S4/10_census.py [--keys k1,k2] [--out log/roadmap/S4/10_census.tsv]
"""
from __future__ import annotations

import argparse
import csv
import json
import pathlib
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port" / "test"))
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT))
from port.test import audio_stream as AS  # noqa: E402
import sound_emu as E  # noqa: E402
import sound_census as SC  # noqa: E402

OUT = ROOT / "log" / "roadmap" / "S4" / "10_census.tsv"


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys")
    ap.add_argument("--out", default=str(OUT))
    ap.add_argument("--budget", type=float, default=60.0)
    a = ap.parse_args(argv)
    keys = a.keys.split(",") if a.keys else AS.exe_keys()
    rows = []
    t0 = time.time()
    with E.FixVM() as vm:
        for n, key in enumerate(keys, 1):
            hooks, batch = E.registry()
            spec = hooks[key]
            res = {}
            for lay in ("A", "B", "C"):
                try:
                    res[lay] = AS.ExeKeyRun(vm, key, lay, budget_s=a.budget).run()
                except E.EmuError as e:
                    res[lay] = e
            r = SC.compare(key, spec, batch.get(key), res, E.read_csv(spec, key))["row"]
            rows.append(r)
            print(f"[{n}/{len(keys)}] {key} {r['tier']} vec {r['vectors']} ret {r['ret_eq']}/{r['ret_cmp']} "
                  f"(run {r['ret_runaddr']}) state {r['state_eq']}/{r['state_cmp']} (run {r['state_runaddr']}) "
                  f"err {r['emu_errors']} {r['first_diff']}", flush=True)
    with open(a.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=SC.COLS, delimiter="\t")
        w.writeheader()
        w.writerows(rows)
    tiers = {}
    for r in rows:
        tiers[r["tier"]] = tiers.get(r["tier"], 0) + 1
    print(f"keys {len(rows)} in {time.time() - t0:.0f} s: " + json.dumps(tiers))


if __name__ == "__main__":
    main()
