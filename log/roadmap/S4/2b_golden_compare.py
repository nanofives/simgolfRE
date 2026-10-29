"""S4.2b: the emulated original's golden digests before (the accepted files, log/roadmap/S4/2b_accepted/port/test and the tree's
golden_audio_core.json) and after the consolidation (S4.2b's oracle --update into port/test; phase 1 wrote log/roadmap/S4/2b_goldens):
per key the digest list, the tier and the regions must be equal; new fields (x87_exit, fpcw, pending, quirk) are reported.

    ~/bin/py -3.12 log/roadmap/S4/2b_golden_compare.py [--new <dir>] [--out <tsv>]
"""
import argparse
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[3]
HERE = pathlib.Path(__file__).resolve().parent
OLD = {"golden_audio_core.json": HERE / "2b_wave1" / "ORIGIN_golden_audio_core.json",
       **{n: HERE / "2b_accepted" / "port" / "test" / n
          for n in ("golden_audio_chan.json", "golden_audio_ds.json", "golden_audio_dev.json", "golden_audio_mixer.json",
                    "golden_audio_codec.json", "golden_audio_seq.json", "golden_audio_sound.json", "golden_audio_wave.json",
                    "golden_audio_soundfile.json", "golden_audio_crt.json", "golden_audio_exe.json")}}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--new", default=str(ROOT / "port" / "test"))
    ap.add_argument("--out")
    a = ap.parse_args()
    rows, bad = [], 0
    for name, op in OLD.items():
        np_ = pathlib.Path(a.new) / name
        if not np_.exists():
            print(f"{name}: no new file")
            continue
        old, new = json.loads(op.read_text()), json.loads(np_.read_text())
        same = diff = 0
        for k in sorted(set(old) | set(new)):
            o, n = old.get(k), new.get(k)
            if o is None or n is None:
                rows.append((name, k, "MISSING_OLD" if o is None else "MISSING_NEW", ""))
                bad += 1
                continue
            why = []
            if "digests" not in o:      # golden_audio_exe.json's `$device` entry: the device-present stream (lines, steps, sha256)
                if o != n:
                    diff += 1
                    bad += 1
                    rows.append((name, k, "DIFF", "entry differs"))
                else:
                    same += 1
                continue
            if o["digests"] != n["digests"]:
                d = [i for i, (x, y) in enumerate(zip(o["digests"], n["digests"])) if x != y]
                why.append(f"digests differ at {d[:8]} of {len(o['digests'])}")
            if o.get("tier") != n.get("tier"):
                why.append(f"tier {o.get('tier')} -> {n.get('tier')}")
            if o.get("regions") != n.get("regions"):
                why.append("regions differ")
            if o.get("quirk", {}) != n.get("quirk", {}):
                why.append(f"quirk {o.get('quirk')} -> {n.get('quirk')}")
            if why:
                diff += 1
                bad += 1
                rows.append((name, k, "DIFF", "; ".join(why)))
            else:
                same += 1
        print(f"{name}: keys old {len(old)} new {len(new)}  equal {same}  differ {diff}")
    for r in rows:
        print("  " + "\t".join(r))
    if a.out:
        with open(a.out, "w") as f:
            f.write("file\tkey\tstatus\tdetail\n" + "".join("\t".join(r) + "\n" for r in rows))
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
