"""S3.1b: G2 of the ORIGINAL's recorded pass streams replayed into GlRaster (glraster.run), every snapshot of the draw-list
corpora (kinds 1, 3, 4) plus c7 / t3 (Terrain::render).
    cd port/build/S3_head && SG_PORTSIM_DLL=<dll> PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/1b_g2_stream.py"""
from port.test import glcorpus as G, glraster as R, lib as L

lib = L.load()
R.bind(lib.dll)
setups, bad, n = {}, 0, 0
snaps = list(G.all_snaps({1, 3, 4})) + [(lb, s) for lb in ("c7", "t3") if (G.CORPUS / lb).exists() for s in G.snaps(G.CORPUS / lb)]
for lb, s in snaps:
    if lb not in setups:
        setups[lb] = R.Setup(G.CORPUS / lb)
    post, got, st, unk, pre = R.run(lib, setups[lb], s)
    rep = R.report(post, got, pre)
    one, big, drawn = rep["diff"] - rep["gt1"], rep["gt1"], max(st[2], 1)
    ok = big <= R.MAX_BIG and one <= R.MAX_ONE_LEVEL * drawn
    bad += not ok
    n += 1
    print(f"{lb} {s} drawn={drawn} one={one} ({100 * one / drawn:.2f}%) big={big} unk={unk} {'OK' if ok else 'FAIL'}", flush=True)
print("PASSES", n, "FAIL", bad)
