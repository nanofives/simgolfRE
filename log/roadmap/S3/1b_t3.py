"""S3.1b: the t3 frames (port/corpus/terrain/t3/frames) replayed into GlRaster: one-level / big per frame (test_terrain_state's
chain draws the same frames from replayed state, pixel-identical to these by its own test).
    cd port/build/S3_head && SG_PORTSIM_DLL=<dll> PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/1b_t3.py"""
from port.test import glcorpus as G, glraster as R, lib as L
from port.test import terraincorpus as TC

lib = L.load()
R.bind(lib.dll)
root = TC.CORPUS / "t3" / "frames"
setup = R.Setup(root)
for s in G.snaps(root):
    post, got, st, unk, pre = R.run(lib, setup, s)
    rep = R.report(post, got, pre)
    one, big, drawn = rep["diff"] - rep["gt1"], rep["gt1"], max(st[2], 1)
    ok = big <= R.MAX_BIG and one <= R.MAX_ONE_LEVEL * drawn
    print(f"t3 {s} drawn={drawn} one={one} ({100 * one / drawn:.2f}%) big={big} unk={unk} {'OK' if ok else 'FAIL'}", flush=True)
