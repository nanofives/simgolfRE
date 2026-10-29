"""S4.10 oracle runner: every exe backend-audio key, port vs the emulated original (port/test/audio_stream.py oracle_key).

    ~/bin/py -3.12 log/roadmap/S4/10_oracle.py [--keys k1,k2] [--layouts ABC] [--out log/roadmap/S4/10_oracle.tsv]
"""
import argparse
import csv
import pathlib
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "re" / "tools"))
from port.test import audio_stream as AS  # noqa: E402

E = AS.E


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys")
    ap.add_argument("--layouts", default="ABC")
    ap.add_argument("--out", default=str(ROOT / "log" / "roadmap" / "S4" / "10_oracle.tsv"))
    a = ap.parse_args(argv)
    keys = a.keys.split(",") if a.keys else AS.exe_keys()
    lib = AS.load_lib()
    rows = []
    t0 = time.time()
    with E.FixVM() as vm:
        for n, k in enumerate(keys, 1):
            v, _, _ = AS.oracle_key(vm, lib, k, tuple(a.layouts))
            rows.append(v)
            print(f"[{n}/{len(keys)}] {k} {v.verdict} match {v.match}/{v.vectors} bc {v.match_bc} err {v.errors} "
                  f"fb {v.fallbacks} exe_fb {v.exe_fallbacks} {v.fallback_names} tier {v.tier} {v.first}", flush=True)
    with open(a.out, "w", newline="") as f:
        w = csv.writer(f, delimiter="\t")
        w.writerow(["key", "name", "verdict", "vectors", "match", "match_bc", "errors", "fallbacks", "exe_fallbacks",
                    "fallback_names", "tier", "first"])
        for v in rows:
            w.writerow([v.key, v.name, v.verdict, v.vectors, v.match, v.match_bc, v.errors, v.fallbacks, v.exe_fallbacks,
                        v.fallback_names, v.tier, v.first])
    c = {}
    for v in rows:
        c[v.verdict] = c.get(v.verdict, 0) + 1
    print(f"keys {len(rows)} vectors {sum(v.vectors for v in rows)} in {time.time() - t0:.0f} s: {c}")


if __name__ == "__main__":
    main()
