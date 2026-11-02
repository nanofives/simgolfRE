"""S3.29: SHA-256 of GlRaster's frame for every recorded pass, stream replays (1b_g2_stream.py's set: draw-list corpora kinds 1 / 3 / 4,
c7, t3) and draw-list chains (1_g2.py's set), with the DLL of $SG_PORTSIM_DLL; `cmp a.json b.json` lists the passes that differ.
Used to show the default renderer of port/build/S3_29 equals the wave 8 stack's (port/build/S3_W8) pass for pass.

    cd <tree> && SG_PORTSIM_DLL=<dll> PYTHONPATH=. ~/bin/py -3.12 ../../../log/roadmap/S3/29_framehash.py dump <out.json>
    ~/bin/py -3.12 log/roadmap/S3/29_framehash.py cmp <a.json> <b.json>
"""
import hashlib
import json
import sys


def dump(out):
    from port.test import glcorpus as G, glraster as R, lib as L
    lib = L.load()
    R.bind(lib.dll)
    res, setups = {}, {}
    snaps = list(G.all_snaps({1, 3, 4})) + [(lb, s) for lb in ("c7", "t3") if (G.CORPUS / lb).exists() for s in G.snaps(G.CORPUS / lb)]
    for lb, s in snaps:
        if lb not in setups:
            setups[lb] = R.Setup(G.CORPUS / lb)
        post, got, st, unk, pre = R.run(lib, setups[lb], s)
        res[f"stream/{lb}/{s}"] = hashlib.sha256(got.tobytes()).hexdigest()
    for lb, s in G.all_snaps({1, 3, 4}):
        post, got, st, pre = R.run_chain(lib, setups[lb], s)
        res[f"chain/{lb}/{s}"] = hashlib.sha256(got.tobytes()).hexdigest()
    open(out, "w").write(json.dumps(res, indent=0, sort_keys=True))
    print("passes", len(res))


def cmp(a, b):
    A, B = json.load(open(a)), json.load(open(b))
    keys = sorted(set(A) | set(B))
    diff = [k for k in keys if A.get(k) != B.get(k)]
    print(f"passes {len(keys)} (a {len(A)}, b {len(B)}): differing {len(diff)}", diff[:20])


if __name__ == "__main__":
    {"dump": dump, "cmp": cmp}[sys.argv[1]](*sys.argv[2:])
