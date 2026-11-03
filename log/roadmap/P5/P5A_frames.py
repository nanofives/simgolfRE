"""P5A item 4: the port's frames inside the F1 course report table and the Information menu's Top 10 Designers page of
ui_tour (clock positions read from port/corpus/traces/ui_tour.trace: F1 key up 0x3552e .. Esc 0x36137; Top 10 click up
0x4c9e2 .. Esc 0x4d623; Best Scores 0x4a388 .. 0x4afc7). Run from a tree root with SG_PORTSIM_DLL set:
    ~/bin/py -3.12 <this> <out dir> [negctl-name=mode]"""
import pathlib, sys
ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT)); sys.path.insert(0, str(ROOT / "re" / "tools"))
import numpy as np
from PIL import Image
import imgcmp
from port.test import boot, gfx
from port.test import lib as L

POS = {"f1_table": 0x36000, "best_scores": 0x4af00, "top10": 0x4d500}
out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
lib = L.load()
neg = [a.split("=") for a in sys.argv[2:]]
for n, v in neg: lib.negctl(n, int(v))
ords = sorted((p, k) for k, p in POS.items())
max_records, to_end = gfx.record_budget("ui_tour", ords[-1][0])
frames = {}
def before(st):
    st.call("gfx.attach", [])
    for o, _k in ords: st.call("gfx.snapshotAt", [o])
def after(st, _res):
    for i, (_o, k) in enumerate(ords):
        raw = st.files_read(f"$gfx/snapshot_{i}.555")
        frames[k] = None if raw is None or len(raw) != gfx.W * gfx.H * 2 else \
            imgcmp.rgb555_to_rgb(np.frombuffer(raw, "<u2").reshape(gfx.H, gfx.W))
    txt = st.files_read("$gfx/log.txt")
    (out / "gfx_log.txt").write_bytes(txt or b"")
    st.call("gfx.detach", [])
res = boot.run("ui_tour", lib, max_records=max_records, before=before, after=after, to_end=to_end)
print("boot", getattr(res, "status", res))
for k, img in frames.items():
    if img is None: print(k, "no snapshot"); continue
    Image.fromarray(img).save(out / f"{k}.png"); print(k, "saved")
