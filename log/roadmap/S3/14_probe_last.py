"""S3.14 probe: replay a kept app session (test_app_live / test_app_ingame layout) with gfx snapshots at its last presents'
positions and report, per position, whether the replay produced a snapshot and whether it equals the present; plus the
replay's code, records and the gfx log's snapshot lines. Run from a mirror root with 14_env.sh sourced.

    ~/bin/py -3.12 ../../../log/roadmap/S3/14_probe_last.py <session dir> [last N presents, default 4] [--budget +K]
"""
import pathlib
import sys

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402
from port.test import live as LV  # noqa: E402
from port.test import terrainlive as TL  # noqa: E402
from port.test import test_app_live as T  # noqa: E402

sess = pathlib.Path(sys.argv[1])
n_last = int(sys.argv[2]) if len(sys.argv) > 2 and not sys.argv[2].startswith("--") else 4
extra = 1
if "--budget" in sys.argv:
    extra = int(sys.argv[sys.argv.index("--budget") + 1])
rec = sess / "rec"
presents = [{"index": int(r[0]), "pos": int(r[2]), "kind": r[3], "frame": int(r[4]), "hash": int(r[6], 16)}
            for r in T._rows(rec / "presents.tsv")]
lib = L.load(str(T.DLL))
if "--negctl" in sys.argv:  # e.g. --negctl tick.frame=12 (S3.14's control: the last frame's pending snapshot left untaken)
    nm, md = sys.argv[sys.argv.index("--negctl") + 1].split("=")
    lib.negctl(nm, int(md))
    print("negctl", nm, md)
nsim = len(lib.objects("sim"))
nrec = len((rec / "framehash.bin").read_bytes()) // (8 + 8 * nsim)
judged, _ = T._judged(presents)
want = judged[-n_last:]
st, done = LV.boot_state(lib, None)
try:
    st.call("gfx.attach", [])
    for p in want:
        st.call("gfx.snapshotAt", [p["pos"]])
    assert TL.live_attach(lib, st, 0) == 0
    trace = (rec / "trace.trace").read_bytes()
    code, n, ts, hs = st.run_boot(trace, nrec + extra)
    print("records in framehash.bin", nrec, "budget", nrec + extra, "-> code", code, "records", n)
    for k, p in enumerate(want):
        raw = st.files_read(f"$gfx/snapshot_{k}.555")
        h = lib.hash_bytes(raw) if raw else None
        print("present", p["index"], "frame", p["frame"], "pos", p["pos"], "snapshot", "none" if raw is None else len(raw),
              "equal" if h == p["hash"] else "DIFFERS")
    st.call("gfx.log", [])
    txt = (st.files_read("$gfx/log.txt") or b"").decode("utf-8", "replace")
    print("\n".join(l for l in txt.splitlines() if "snap" in l.lower())[-3000:])
finally:
    done()
