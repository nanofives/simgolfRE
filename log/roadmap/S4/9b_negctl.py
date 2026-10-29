"""S4.9b negative controls: every mode of snd.loadSoundFile (audio_soundfile.cpp kNeg*) against the pinned goldens. Default: the bundle
replay (port/corpus/audio, the recorded host script; a port request the script does not expect stops the vector, which counts as a
difference); --live: the live oracle on layout A (the emulated original as the host). RED = at least one vector differs.

    ~/bin/py -3.12 log/roadmap/S4/9b_negctl.py [--live] [--modes 1 2 ...] [--w32] [--out tsv]
"""
import argparse
import csv
import importlib.util
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s49b_replay", HERE / "9b_replay.py")
RUN = importlib.util.module_from_spec(spec)
spec.loader.exec_module(RUN)
R, E = RUN.R, RUN.E

MODES = {1: "return ^ 1", 2: "ctrl: readFmtChunk skipped", 3: "trak: node value NULL", 4: "glbl: Voice_setPlayModeBits skipped",
         5: "chnl: Seq_copyChannelParams skipped", 6: "kmap: trailing dword not stored", 7: "prgm: 0x1c flag stays 0",
         8: "MThd: Seq_applyTrackFlag skipped", 9: "prefix: own-name cut skipped", 10: "unknown tag: g_openMmioCount kept"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--live", action="store_true")
    ap.add_argument("--w32", action="store_true")
    ap.add_argument("--modes", nargs="*", type=int, default=sorted(MODES))
    ap.add_argument("--out")
    a = ap.parse_args()
    golden = R.load_golden(RUN.STEP)      # port/test/sound_replay.py since S4.2b
    lib = R.load_w32() if a.w32 else R.load_lib()
    keys = RUN.family_keys("soundfile")
    rows = []
    vm = E.FixVM() if a.live else None
    try:
        for n, mode in enumerate(a.modes, 1):
            lib.negctl(RUN.PORT_NAME, mode)
            bad, tot, where = 0, 0, []
            try:
                for k in keys:
                    if a.live:
                        v, _, _ = R.oracle_key(vm, lib, k, ("A",), record=False)
                        d, t = v.vectors - v.match, v.vectors
                    else:
                        r = RUN.replay_key(lib, k, golden)
                        d, t = r.vectors - r.passed, r.vectors
                    bad += d
                    tot += t
                    if d:
                        where.append(f"{k.split('@')[1]}:{d}")
            finally:
                lib.negctl(RUN.PORT_NAME, 0)
            verdict = "RED" if bad else "BLIND"
            rows.append((mode, MODES.get(mode, ""), verdict, bad, tot, " ".join(where)))
            print(f"[{n}/{len(a.modes)}] mode {mode} ({MODES.get(mode, '')}) {verdict} {bad}/{tot} vectors differ  {' '.join(where)}",
                  flush=True)
    finally:
        if vm is not None:
            vm.close()
    if a.out:
        with open(a.out, "w", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["mode", "what", "verdict", "vectors_differ", "vectors", "keys"])
            w.writerows(rows)
    blind = [r[0] for r in rows if r[2] != "RED"]
    print(f"modes {len(rows)}  red {len(rows) - len(blind)}  blind {len(blind)} {blind}")
    return 1 if blind else 0


if __name__ == "__main__":
    sys.exit(main())
