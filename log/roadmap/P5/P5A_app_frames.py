"""P5A: rebuild an app session's presents by replaying its recording with gfx snapshots at their positions (as
port/test/test_app_live.py _hover_frames does; no terrain needed on the world map). Run from a tree root:
    ~/bin/py -3.12 <this> <session dir> <out dir> <pos> [<pos> ...]"""
import pathlib, sys
ROOT = pathlib.Path.cwd(); sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(ROOT / "re" / "tools"))
import numpy as np
from PIL import Image
import imgcmp
from port.test import lib as L
from port.test import live as LV
from port.test import app_replay as A

rec, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]); out.mkdir(parents=True, exist_ok=True)
pos = sorted(int(p) for p in sys.argv[3:])
lib = L.load()
nrec = len(A.read_framehash((rec / "framehash.bin").read_bytes(), len(lib.objects("sim"))))
st, done = LV.boot_state(lib, None)
try:
    st.call("gfx.attach", [])
    for p in pos: st.call("gfx.snapshotAt", [p])
    st.run_boot((rec / "trace.trace").read_bytes(), nrec + 1)
    for k, p in enumerate(pos):
        raw = st.files_read(f"$gfx/snapshot_{k}.555")
        if raw is None: print(p, "no snapshot"); continue
        Image.fromarray(imgcmp.rgb555_to_rgb(np.frombuffer(raw, "<u2").reshape(600, 800))).save(out / f"{rec.name}_{p}.png")
        print(p, hex(lib.hash_bytes(raw)))
finally:
    done()
