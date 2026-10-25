"""S3.2 probe: the recorded root surface (frames/<cp>/exit.bin) against the checkpoint's reference frame, both row orders."""
import sys, pathlib, numpy as np
ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT))
from port.test import gfx as GX
rec, scen = sys.argv[1], sys.argv[2]
for d in sorted((ROOT / "log/p5/terrain" / rec / "frames").iterdir()):
    f = d / "exit.bin"
    if not f.exists():
        continue
    a = np.frombuffer(f.read_bytes(), "<u2").reshape(600, 800)
    rgb = lambda v: np.stack([((v >> 10) & 31) << 3 | ((v >> 10) & 31) >> 2, ((v >> 5) & 31) << 3 | ((v >> 5) & 31) >> 2, (v & 31) << 3 | (v & 31) >> 2], -1).astype(np.uint8)
    ref = GX.reference(scen, d.name)
    for nm, im in (("memrow0=top", rgb(a)), ("memrow0=bottom", rgb(a[::-1]))):
        eq = (im == ref).all(-1)
        print(d.name, nm, "equal px", int(eq.sum()), "of", eq.size)
