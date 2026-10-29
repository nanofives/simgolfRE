"""S4.11: the other steps' golden files before (the tree's port/test/golden_audio_*.json) and after audio_crt joined the port
(11_census2.sh writes log/roadmap/S4/11_goldens): per key the digest list, the tier, the regions and the x87 exit digests must be
equal (the emulated original did not change); the fallback / pending fields are REPORTED (they are what this step removes).

    ~/bin/py -3.12 log/roadmap/S4/11_golden_compare.py [--new <dir>] [--out <tsv>]
"""
import argparse
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[3]
HERE = pathlib.Path(__file__).resolve().parent
NAMES = ("core", "chan", "ds", "dev", "mixer", "codec", "seq", "sound", "wave", "soundfile")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--new", default=str(HERE / "11_goldens"))
    ap.add_argument("--old", default=str(ROOT / "port" / "test"))
    ap.add_argument("--out")
    a = ap.parse_args()
    rows, bad = [], 0
    tot_old_fb = tot_new_fb = keys = 0
    for n in NAMES:
        name = f"golden_audio_{n}.json"
        op, np_ = pathlib.Path(a.old) / name, pathlib.Path(a.new) / name
        if not op.exists() or not np_.exists():
            print(f"{name}: old {op.exists()} new {np_.exists()}")
            continue
        old, new = json.loads(op.read_text()), json.loads(np_.read_text())
        for k in sorted(set(old) | set(new)):
            o, nw = old.get(k), new.get(k)
            if o is None or nw is None:
                if nw is None:
                    rows.append((name, k, "MISSING_NEW", ""))
                    bad += 1
                continue
            keys += 1
            why = [f for f in ("digests", "tier", "regions", "x87_exit", "vectors") if o.get(f) != nw.get(f)]
            fo, fn = o.get("fallbacks") or 0, nw.get("fallbacks") or 0
            tot_old_fb += fo
            tot_new_fb += fn
            if why:
                bad += 1
            rows.append((name, k, "DIFF " + ",".join(why) if why else "SAME", f"fallbacks {fo} -> {fn}" + (" pending" if nw.get("pending") else "")))
    for r in rows:
        if r[2] != "SAME" or "pending" in r[3]:
            print("\t".join(r))
    print(f"keys {keys}  differing {bad}  fallbacks {tot_old_fb} -> {tot_new_fb}")
    if a.out:
        with open(a.out, "w", newline="") as f:
            for r in rows:
                f.write("\t".join(r) + "\n")
    return 1 if bad or tot_new_fb else 0


if __name__ == "__main__":
    raise SystemExit(main())
