import sys, time
from port.test import glcorpus as G, glraster as R, lib as L
lib = L.load()
setups = {}
bad = 0
for lb, s in G.all_snaps({1, 3, 4}):
    if lb not in setups: setups[lb] = R.Setup(G.CORPUS / lb)
    post, got, st, pre = R.run_chain(lib, setups[lb], s)
    rep = R.report(post, got, pre)
    one, big, drawn = rep["diff"] - rep["gt1"], rep["gt1"], max(st[2], 1)
    ok = big <= R.MAX_BIG and one <= R.MAX_ONE_LEVEL * drawn
    bad += not ok
    print(f"{lb} {s} kind={G.meta(G.CORPUS/lb/'snaps'/s/'meta.txt')['entry_kind']} drawn={drawn} changed={rep['changed']} one={one} ({100*one/drawn:.2f}%) big={big} {'OK' if ok else 'FAIL'}", flush=True)
print("FAIL", bad)
