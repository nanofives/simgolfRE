"""Part (d) of step S4.1: Unicorn's x87 precision / rounding control on the c3fh caller-word keys (sound_replay.md).

The c3fh keys ending in _w27f / _wf7f run their vectors with the A/B thread's control word set by the fixture to 0x027f
(53-bit precision, round to nearest) or 0x0f7f (64-bit precision, round toward zero). For each such sound.dll key:

  own      the replay as the fixture sets it: fields equal to the CSV (return, state hash, state_changed) and comparable ones
  emu      vectors whose return or state bytes in the emulator change with the swap (the CSV cannot show these when the
           regions hold run addresses; they are what a port has to reproduce at both words)
  swapped  the same replay with the control word changed to the other value right after the fixture: how many of the fields
           that matched now differ. A key whose fields all survive the swap does not observe the control word on its vectors
           (no evidence either way); one that loses fields shows that the emulator's results at each word are the live ones.

    ~/bin/py -3.12 re/tools/fixvm/x87_cw_measure.py [--out log/roadmap/S4/1_x87_cw.tsv]
"""
from __future__ import annotations

import argparse
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port" / "test"))
sys.path.insert(0, str(pathlib.Path(__file__).parent))

import sound_emu as E  # noqa: E402
from controls import matched_fields  # noqa: E402
from unicorn.x86_const import UC_X86_REG_FPCW  # noqa: E402

WORDS = {"w27f": 0x027F, "wf7f": 0x0F7F}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "log" / "roadmap" / "S4" / "1_x87_cw.tsv"))
    a = ap.parse_args()
    hooks, batch = E.registry()
    keys = [k for k in E.sound_keys() if batch.get(k) == "c3fh" and k.rsplit("_", 1)[-1] in WORDS]
    rows = []
    with E.FixVM() as vm:
        for n, k in enumerate(keys, 1):
            spec = hooks[k]
            csv = E.read_csv(spec, k)
            own = k.rsplit("_", 1)[-1]
            other = WORDS["wf7f" if own == "w27f" else "w27f"]
            base = E.KeyRun(vm, k, "A").run()
            good = matched_fields(base, csv)
            nrows = len(base.rows)

            def patch(run, fx, w=other):
                run.m.uc.reg_write(UC_X86_REG_FPCW, w)
            sw = E.KeyRun(vm, k, "A", fixture_patch=patch).run()
            now = {(i, f): v for i, f, v in matched_fields(sw, csv)}
            lost = [(i, f) for i, f, v in good if now.get((i, f)) != v]
            errs = sum(1 for r in sw.rows if r.error)
            kinds = sorted({f for _, f in lost})
            # emulator against itself: vectors whose state bytes or return change with the word (the CSV cannot show it
            # when the regions hold run addresses, but the port must reproduce it)
            emu_diff = sum(1 for x, y in zip(base.rows, sw.rows) if (x.state_hex, x.orig) != (y.state_hex, y.orig))
            r = {"key": k, "word": f"0x{WORDS[own]:04x}", "vectors": nrows, "matched_own": len(good),
                 "lost_swapped": len(lost), "lost_fields": ",".join(kinds), "errors_swapped": errs, "emu_rows_changed": emu_diff,
                 "verdict": "SENSITIVE" if lost or errs else "BLIND"}
            rows.append(r)
            print(f"[{n}/{len(keys)}] {k} {r['verdict']} matched {len(good)} at {r['word']}, lost {len(lost)} with the word "
                  f"swapped ({r['lost_fields']}); emulator rows changed by the swap {emu_diff}/{nrows}", flush=True)
    cols = ["key", "word", "vectors", "matched_own", "lost_swapped", "lost_fields", "errors_swapped", "emu_rows_changed", "verdict"]
    pathlib.Path(a.out).write_text("\t".join(cols) + "\n" + "".join("\t".join(str(r[c]) for c in cols) + "\n" for r in rows))


if __name__ == "__main__":
    sys.exit(main())
