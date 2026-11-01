r"""S3.27: the port's draw stream (port.test.glcorpus.render) against the original's pass lines for every kept pass of a
27_gl_record.py corpus. Run from a built tree: cd port/build/S3_26; SG_PORTSIM_DLL=... PYTHONPATH=. python <this> [label] [step...]"""
import pathlib, sys
sys.path.insert(0, str(pathlib.Path.cwd()))
from port.test import lib as L, glcorpus as G
MAIN = pathlib.Path(__file__).resolve().parents[3]
label = sys.argv[1] if len(sys.argv) > 1 else "s327_s2"
root = MAIN / "log/p5/gl/corpus" / label
steps = sys.argv[2:] or sorted(p.name for p in (root / "snaps").iterdir())
lib = L.load(rebuild=False)
for s in steps:
    sn = G.Snap(root, s)
    try:
        got = G.render(lib, sn)
    except Exception as e:
        print(f"{s:20s} ERROR {e}"); continue
    d = G.first_difference(got, sn.expected)
    print(f"{s:20s} port {len(got):6d} orig {len(sn.expected):6d} " + ("EQUAL" if d is None else f"DIFF at {d[0]}: port {d[1]!r} orig {d[2]!r}"), flush=True)
