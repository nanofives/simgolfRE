"""S10.12 step 1: residual list. Every address of a MATCH / WIP line of re/match/wip/*.cpp that no canonical
re/match/*.cpp scores 100% (default compile of each canonical file that names it, no evidence).

    ~/bin/py -3.12 log/roadmap/S10/12_tools/residual.py [--out log/roadmap/S10/12_residual_raw.tsv]
"""
import argparse, contextlib, io, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402

LINE = re.compile(r"^\s*//\s*(MATCH|WIP):\s*(\S+)\s+0x([0-9a-fA-F]+)\s*(\S*)", re.M)


def wip_lines():
    out = {}
    for f in sorted((ROOT / "re/match/wip").glob("*.cpp")):
        for kind, mod, addr, sym in LINE.findall(f.read_text(errors="replace")):
            out.setdefault((mod, int(addr, 16)), []).append((f.name, kind, sym))
    return out


def canonical_files(cands):
    files = {}
    for f in sorted((ROOT / "re/match").glob("*.cpp")):
        for kind, mod, addr, sym in LINE.findall(f.read_text(errors="replace")):
            if kind == "MATCH" and (mod, int(addr, 16)) in cands:
                files.setdefault(f, set()).add((mod, int(addr, 16)))
    return files


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="log/roadmap/S10/12_residual_raw.tsv")
    a = ap.parse_args()
    cands = wip_lines()
    print(f"{len(cands)} candidate addresses", flush=True)
    best = {}
    files = canonical_files(cands)
    for k, (f, keys) in enumerate(files.items(), 1):
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                res = match.compare(f, "", False, quiet=True)
        except SystemExit as e:
            print(f"[{k}/{len(files)}] {f.name} ERR {str(e)[-60:]}", flush=True)
            continue
        for r in res:
            if (r[0], r[1]) in cands:
                best[(r[0], r[1])] = max(best.get((r[0], r[1]), 0), r[4])
        print(f"[{k}/{len(files)}] {f.name} ok", flush=True)
    rows = []
    for key, where in sorted(cands.items()):
        sc = best.get(key)
        if sc is not None and sc >= 0.99995:
            continue
        rows.append([key[0], f"0x{key[1]:08x}", "none" if sc is None else f"{100*sc:.2f}", ",".join(sorted({w[0] for w in where}))])
    pathlib.Path(ROOT / a.out).write_text("module\taddr\tcanonical_best\twip_files\n" + "".join("\t".join(r) + "\n" for r in rows))
    print(f"{len(rows)} residual rows -> {a.out}")


if __name__ == "__main__":
    main()
