"""S7 lead: every accepted segment with switch sound.golfSfxAlias at 0 (the integrator has no switch option), one process,
x64 (default) or win32 (SG_BACKEND=win32), status per segment against frames.bin; TSV to argv[1].
Usage (from port/build/S7_12_root): ~/bin/py -3.12 <this> <out.tsv>"""
import pathlib
import sys
import time

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402
from port.test import segments as S  # noqa: E402

lib = L.load()
assert "sound.golfSfxAlias" in lib.defects(), "no switch in this build"
lib.defect("sound.golfSfxAlias", False)
assert lib.defects()["sound.golfSfxAlias"]["on"] == 0
runner = S.SegmentRunner(lib)
rows = [r for r in S.manifest_rows() if r.get("status", "ACCEPTED").upper().startswith("ACCEPT")]
out = open(sys.argv[1], "w", encoding="utf-8")
out.write("name\tstatus\tframes_run\tframes_expected\tdetail\n")
counts = {}
t0 = time.time()
for i, r in enumerate(rows, 1):
    d = S.segment_dir(r)
    name = f"{r['scenario']}/{int(r['segment']):02d}"
    res = runner.run(S.load_segment(d))
    counts[res.status] = counts.get(res.status, 0) + 1
    out.write(f"{name}\t{res.status}\t{res.frames_run}\t{res.frames_expected}\t{res.detail[:200]}\n")
    out.flush()
    print(f"[{i}/{len(rows)}] {name} {res.status}", flush=True)
lib.defect_reset()
print({"switch": "sound.golfSfxAlias=0", "counts": counts, "wall_s": round(time.time() - t0, 1)}, flush=True)
