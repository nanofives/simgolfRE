"""S3.28: which kind-5 passes (lowerEdgeCorner's draw half) write pixels, and whether a full render follows before the checkpoints.

    bash log/roadmap/S3/28_passlog.sh             # framegate (no split) per scenario with SG_TERRAIN_PASS_LOG=28_out/passlog_<s>.txt
    ~/bin/py -3.12 log/roadmap/S3/28_passlog.py   # summary per scenario -> 28_out/passlog_summary.tsv

Pass log line (gfx_terrain.cpp): <updateTerrainRender call k> <pass j in it, -1 for kind 5> <entry kind> <us> <px written> <triangles>
<fast px> <workers>. For every kind-5 pass with pixels: its frame k and the first later frame holding a Terrain::render (kind 2) pass,
which starts from Window::paint's cleared root, so the silhouette cannot outlast it.
"""
import pathlib

H = pathlib.Path(__file__).resolve().parent
O = H / "28_out"


def main():
    out = ["scenario\tkind5_passes\tkind5_with_px\tpx_total\tframes_with_px\tnext_render_frame_after_last\tlast_frame"]
    for p in sorted(O.glob("passlog_*.txt")):
        s = p.stem[len("passlog_"):]
        rows = [ln.split() for ln in p.read_text().splitlines() if ln.strip()]
        rows = [(int(r[0]), int(r[1]), int(r[2]), int(r[4])) for r in rows if len(r) >= 5]
        k5 = [r for r in rows if r[2] == 5]
        px = [r for r in k5 if r[3] > 0]
        renders = sorted({r[0] for r in rows if r[2] == 2})
        last = max((r[0] for r in px), default=-1)
        nxt = next((f for f in renders if f > last), -1) if px else -1
        out.append(f"{s}\t{len(k5)}\t{len(px)}\t{sum(r[3] for r in px)}\t{','.join(str(r[0]) for r in px)}\t{nxt}\t{max((r[0] for r in rows), default=-1)}")
    (O / "passlog_summary.tsv").write_text("\n".join(out) + "\n")
    print("\n".join(out))


if __name__ == "__main__":
    main()
