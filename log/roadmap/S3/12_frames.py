"""S3.12 copy of 10_frames.py (tree = $SG_TREE, default port/build/S3_12). S3.10: one line per recorded GL pass with the SHA-1 of the frame a build's rasteriser writes and its pixel counters, so two builds
(the old rasteriser and this step's) can be compared exactly in separate processes:

    SG_PORTSIM_DLL=<dll> ~/bin/py -3.12 log/roadmap/S3/10_frames.py <out.tsv> [--line "#threads 4"]
    ~/bin/py -3.12 log/roadmap/S3/10_frames.py --compare <a.tsv> <b.tsv>

Every snapshot of port/corpus/gl/* is replayed as port/test/glraster.run does (stream path); the render-entry snapshots also through
sg_gl_render_to_raster (chain path).
"""
import argparse
import hashlib
import os
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, os.environ.get("SG_TREE") or str(ROOT / "port" / "build" / "S3_12"))   # S3.12: the tree is a parameter


def dump(out, line):
    from port.test import glcorpus as G
    from port.test import glraster as R
    from port.test import lib as L
    lib = L.load(os.environ["SG_PORTSIM_DLL"])

    def patch(r, dll):
        if line:
            dll.sg_raster_exec(r, line.encode(), len(line))
    rows = []
    for root in sorted(p for p in G.CORPUS.iterdir() if (p / "snaps").is_dir()):
        setup = R.Setup(root)
        for step in G.snaps(root):
            post, got, st, unknown, pre = R.run(lib, setup, step, patch=patch)
            rows.append(f"{root.name}\t{step}\tstream\t{hashlib.sha1(got.tobytes()).hexdigest()}\t{','.join(map(str, st[:5]))}")
            if G.meta(root / "snaps" / step / "meta.txt").get("entry_kind") == G.ENTRY_RENDER:
                post, got, st, pre = R.run_chain(lib, setup, step, patch=patch)
                rows.append(f"{root.name}\t{step}\tchain\t{hashlib.sha1(got.tobytes()).hexdigest()}\t{','.join(map(str, st[:5]))}")
            print(rows[-1], flush=True)
    pathlib.Path(out).write_text("label\tstep\tpath\tsha1\tstats\n" + "\n".join(rows) + "\n")


def compare(a, b):
    ra = [l.split("\t") for l in pathlib.Path(a).read_text().splitlines()[1:]]
    rb = {(r[0], r[1], r[2]): r for r in (l.split("\t") for l in pathlib.Path(b).read_text().splitlines()[1:])}
    bad = [r[:3] for r in ra if rb.get((r[0], r[1], r[2])) != r]
    print(f"{len(ra)} rows of {a}, {len(rb)} of {b}: {len(bad)} differ (frame SHA-1 or counters)")
    for x in bad[:20]:
        print("DIFF", x)
    return 1 if bad or len(ra) != len(rb) else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", nargs="?")
    ap.add_argument("--line", default="")
    ap.add_argument("--compare", nargs=2)
    a = ap.parse_args()
    if a.compare:
        return compare(*a.compare)
    dump(a.out, a.line.replace("\\n", "\n") + ("\n" if a.line and not a.line.endswith("\n") else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
