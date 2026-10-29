"""Lead S10: score re/match files with re/match/boundaries.tsv plus the S10 boundaries proposals (scratch copy).

usage: bound_sim.py <out.tsv> <shard k> <shards n>   (no evidence written; re/match/boundaries.tsv untouched)
"""
import pathlib, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402

S10 = ROOT / "log" / "roadmap" / "S10"
PROPOSED = [S10 / "9_boundaries.tsv", S10 / "6_boundaries_sound.tsv", S10 / "7_boundaries.tsv"]


def scratch_boundaries() -> pathlib.Path:
    lines = (ROOT / "re" / "match" / "boundaries.tsv").read_text().splitlines()
    for p in PROPOSED:
        for line in p.read_text().splitlines():
            c = line.split("\t")
            if len(c) >= 3 and c[1].startswith("0x"):
                lines.append("\t".join(c[:4]))
    out = pathlib.Path(tempfile.mkdtemp(prefix="s10b_")) / "boundaries.tsv"
    out.write_text("\n".join(lines) + "\n")
    return out


def main():
    out, k, n = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    match.BOUNDARIES = scratch_boundaries()
    files = sorted((ROOT / "re" / "match").glob("*.cpp"))[k::n]
    with open(out, "w", encoding="utf-8") as f:
        for src in files:
            try:
                res = match.compare(src, "", False)
            except SystemExit as e:
                f.write(f"{src.name}\tERROR\t{e}\n"); continue
            except Exception as e:  # noqa: BLE001
                f.write(f"{src.name}\tERROR\t{type(e).__name__}: {e}\n"); continue
            for r in res:
                f.write(f"{src.name}\t{r[0]}\t{hex(r[1])}\t{100 * r[4]:.1f}\n")
            f.flush()


if __name__ == "__main__":
    main()
