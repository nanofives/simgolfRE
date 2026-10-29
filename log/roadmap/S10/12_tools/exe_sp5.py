"""S10.12: score every exe MATCH line of canonical re/match/<glob> with --toolchain sp5 (no evidence); list the lines
that are not 100%. ~/bin/py -3.12 log/roadmap/S10/12_tools/exe_sp5.py [--glob 'smsg_golf_*.cpp'] [--tc sp5]
Output: log/roadmap/S10/12_exe_<tc>.tsv (file, addr, symbol, ins, score_default, score_tc)."""
import argparse, contextlib, io, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402


def run(f, tc):
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            return {(r[0], r[1]): (r[2], r[4], max(len(r[5]), len(r[6]))) for r in match.compare(f, "", False, quiet=True, toolchain=tc)}
    except SystemExit as e:
        return str(e).splitlines()[-1][:80]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--glob", default="smsg_golf_*.cpp")
    ap.add_argument("--tc", default="sp5")
    a = ap.parse_args()
    files = sorted((ROOT / "re/match").glob(a.glob))
    rows, bad = [], 0
    for k, f in enumerate(files, 1):
        res = run(f, a.tc)
        if isinstance(res, str):
            print(f"[{k}/{len(files)}] {f.name} ERR {res}", flush=True)
            rows.append([f.name, "", "", "", "", "err: " + res])
            continue
        low = [(key, v) for key, v in res.items() if v[1] < 0.99995]
        bad += len(low)
        for key, v in low:
            rows.append([f.name, f"0x{key[1]:08x}", v[0], str(v[2]), "100.0", f"{100 * v[1]:.2f}"])
        print(f"[{k}/{len(files)}] {f.name} lines={len(res)} below100={len(low)}", flush=True)
    out = ROOT / f"log/roadmap/S10/12_exe_{a.tc}.tsv"
    out.write_text("file\taddr\tsymbol\tins\tdefault\t" + a.tc + "\n" + "".join("\t".join(r) + "\n" for r in rows))
    print(f"{bad} lines below 100% under {a.tc} -> {out.name}")


if __name__ == "__main__":
    main()
