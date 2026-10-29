"""S10.5: score every sound.dll MATCH line of re/match/wip/*.cpp with each toolchain (no evidence written).

    ~/bin/py -3.12 log/roadmap/S10/5_tools/nearmiss.py [--glob 'wip/*.cpp'] [--out log/roadmap/S10/5_nearmiss.tsv]

Columns: file addr symbol ins_8168 score_8168 score_sp5 score_sp5c2 (score in %, `err: <first line>` on a compile error).
"""
import argparse
import contextlib
import io
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402

TCS = ["8168", "sp5", "sp5c2"]


def scores(src: pathlib.Path, tc: str):
    """`// WIP: <module> <addr> <symbol> [score]` lines (m36a's near misses) are scored as MATCH lines of a scratch
    copy beside the file (its relative includes keep working), deleted afterwards."""
    text = src.read_text()
    if re.search(r"^\s*//\s*WIP:", text, re.M):
        tmp = src.with_name(f"_s105_{src.name}")
        tmp.write_text(re.sub(r"^(\s*)//\s*WIP:\s*(\S+\s+0x[0-9a-fA-F]+\s+\S+).*$", r"\1// MATCH: \2", text, flags=re.M))
        try:
            return scores(tmp, tc)
        finally:
            tmp.unlink()
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            res = match.compare(src, "", False, quiet=True, toolchain=tc)
    except SystemExit as e:
        return f"err: {str(e).splitlines()[-1][:80]}"
    return {(r[0], r[1]): (r[2], r[4], max(len(r[5]), len(r[6]))) for r in res}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--glob", default="wip/*.cpp")
    ap.add_argument("--out", default="log/roadmap/S10/5_nearmiss.tsv")
    a = ap.parse_args()
    files = [f for f in sorted((ROOT / "re" / "match").glob(a.glob)) if re.search(r"(MATCH|WIP): sound\.dll", f.read_text())
             and not f.name.startswith("_s105_")]
    rows = []
    for k, f in enumerate(files, 1):
        per = {tc: scores(f, tc) for tc in TCS}
        base = per["8168"]
        keys = list(base) if isinstance(base, dict) else [("sound.dll", None)]
        for key in keys:
            cells = []
            for tc in TCS:
                v = per[tc]
                cells.append(v if isinstance(v, str) else f"{100 * v[key][1]:.1f}" if key in v else "missing")
            sym, ins = (base[key][0], base[key][2]) if isinstance(base, dict) else ("", "")
            addr = f"0x{key[1]:08x}" if key[1] is not None else ""
            rows.append([f.relative_to(ROOT).as_posix(), addr, sym, str(ins), *cells])
        print(f"[{k}/{len(files)}] {f.name} " + " ".join(f"{tc}={per[tc] if isinstance(per[tc], str) else 'ok'}"
                                                         for tc in TCS), flush=True)
    out = ROOT / a.out
    out.write_text("\t".join(["file", "addr", "symbol", "ins", *[f"score_{t}" for t in TCS]]) + "\n"
                   + "".join("\t".join(r) + "\n" for r in rows))
    print(f"{len(rows)} rows -> {out.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
