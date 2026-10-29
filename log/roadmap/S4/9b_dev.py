"""S4.9b development: one key, layout A, the port as the second arm; prints per vector the match, the port's error and audio_core's
report (detail, fallback rvas) and, for a mismatch, the first differing state region and byte.

    ~/bin/py -3.12 log/roadmap/S4/9b_dev.py <key> [--vectors i ...]
"""
import argparse
import importlib.util
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s49b_replay", HERE / "9b_replay.py")
RUN = importlib.util.module_from_spec(spec)
spec.loader.exec_module(RUN)
R, E = RUN.R, RUN.E


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("key")
    ap.add_argument("--layout", default="A")
    a = ap.parse_args()
    lib = R.load_lib()
    with E.FixVM() as vm:
        lp = R.LivePort(lib, a.key, True)
        try:
            res = RUN.KeyRun8(vm, a.key, a.layout, second_arm=lp).run()
        finally:
            lp.close()
    names = [s[0] for s in R.spec_of(a.key)["state"]]
    print("fixture error:", res.fixture_error)
    for i, row in enumerate(res.rows):
        rec = lp.records[i] if i < len(lp.records) else None
        rep = rec.report if rec else {}
        print(f"v{i} {row.vector} orig {row.orig} port {row.re} match {row.match} err {row.error}")
        if rec is not None and (rec.error or not row.match):
            print("    port error:", rec.error, "| detail:", rep.get("detail"), "| fallbacks:", rep.get("fallback_rvas"))
        if row.state_hex and row.state_re_hex and row.state_hex != row.state_re_hex:
            for k, (x, y) in enumerate(zip(row.state_hex.split("|"), row.state_re_hex.split("|"))):
                if x != y:
                    bx, by = bytes.fromhex(x), bytes.fromhex(y)
                    j = next(n for n in range(min(len(bx), len(by))) if bx[n] != by[n])
                    print(f"    region {names[k]} first diff at +0x{j:x}: orig {bx[j:j + 16].hex()} port {by[j:j + 16].hex()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
