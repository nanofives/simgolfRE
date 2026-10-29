"""S4.9b's replay front end, pointed back at port/test/sound_replay.py (S4.2b). Until S4.2b it was a copy of 9_replay.py over the
snapshots in log/roadmap/S4/9b_snap (kept as the record of the accepted step); the unified runner now holds what it added: the 25
custom keys (sound_replay CUSTOM, fixtures port/test/sound_fixtures/s49b_soundfile.js), the user's own files (user_files_js),
the golden file golden_audio_soundfile.json (STEPS["S4.9b"]), the mmio sinklog and the ten negative-control modes
(NEGCTL_MODES["loadSoundFile"]). Every command forwards to `sound_replay <cmd> --step S4.9b`; `orig` runs the original alone.

    ~/bin/py -3.12 log/roadmap/S4/9b_replay.py orig    [--keys k ...]
    ~/bin/py -3.12 log/roadmap/S4/9b_replay.py oracle  [--keys k ...] [--update] [--out tsv]
    ~/bin/py -3.12 log/roadmap/S4/9b_replay.py replay  [--w32]
    ~/bin/py -3.12 log/roadmap/S4/9b_replay.py negctl  [--mode m] [--out tsv]
    ~/bin/py -3.12 log/roadmap/S4/9b_replay.py sinklog

9b_cover.py, 9b_negctl.py and 9b_dev.py import this file for R, E, KeyRun8, family_keys, replay_key, GOLDENS and PORT_NAME.
"""
from __future__ import annotations

import argparse
import pathlib
import struct
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT))
from port.test import sound_replay as R  # noqa: E402

E = R.E
STEP = "S4.9b"
PORT_NAME = "snd.loadSoundFile"
GOLDENS = {"soundfile": R.golden_path(STEP)}
KeyRun8 = E.KeyRun          # sound_replay patches KeyRun with the custom specs and fixtures


def family_keys(family: str = "soundfile") -> list[str]:
    return R.keys_for(STEP)


def replay_key(lib, key, golden, **kw):
    return R.replay_key(lib, key, golden, **kw)


def orig(keys):
    """The original alone (one arm) per key: per vector the return, the error, the heap stand-in's cursor and call counter, the free
    log's count and the import log's record count (development: do the fixtures work, which exit does each vector take)."""
    with E.FixVM() as vm:
        for n, key in enumerate(keys, 1):
            try:
                res = KeyRun8(vm, key, "A", arms=1).run()
            except Exception as e:  # noqa: BLE001
                print(f"[{n}/{len(keys)}] {key} ERROR {type(e).__name__}: {e}", flush=True)
                continue
            if res.fixture_error:
                print(f"[{n}/{len(keys)}] {key} FIXTURE {res.fixture_error}", flush=True)
                continue
            names = [s[0] for s in R.spec_of(key)["state"]]
            print(f"[{n}/{len(keys)}] {key} vectors {len(res.rows)}", flush=True)
            for i, row in enumerate(res.rows):
                extra = ""
                if row.state_hex:
                    parts = row.state_hex.split("|")
                    ar = bytes.fromhex(parts[names.index("$arena")])
                    fl = bytes.fromhex(parts[names.index("$freelog")])
                    recs, lost = R.decode_implog(bytes.fromhex(parts[names.index("$implog")]))
                    cur, calls = struct.unpack_from("<II", ar, 0)
                    extra = (f" | arena 0x{cur:x} allocs {calls} frees {struct.unpack_from('<I', fl, 0)[0]} imports {len(recs)}"
                             f" lost {lost}")
                print(f"    v{i} {row.vector} -> {row.orig} {row.error or ''}{extra}", flush=True)
    return 0


def main(argv=None):
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv[:1] == ["orig"]:
        ap = argparse.ArgumentParser()
        ap.add_argument("cmd")
        ap.add_argument("--keys", nargs="*")
        a = ap.parse_args(argv)
        return orig(a.keys or family_keys())
    argv = [x for i, x in enumerate(argv) if not (x == "--family" or (i and argv[i - 1] == "--family"))]
    return R.main(argv + ["--step", STEP])


if __name__ == "__main__":
    sys.exit(main())
