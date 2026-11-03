"""S3 wave 10 base tree: the port tree of I22's mirror (log/roadmap/merges/I22/make_mirror.py) without its documents:
`git archive d075dc56` + the junctions (w7_assemble.py --plain), then S3 wave 9 rows 4 and 5 (28_assemble.py --only-28,
29_assemble.py --only-29). Lead, 2026-11-01.

    ~/bin/py -3.12 log/roadmap/S3/w10_base.py --dst S3_30          # port/build/S3_30
    ~/bin/py -3.12 log/roadmap/S3/w10_base.py --verify S3_30       # byte compare its port/ with port/build/I22 (read only)

Never write into port/build/I22. Build only in your own tree (24_build.py <arch> --tree <dst>)."""
import argparse
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
BASE = "d075dc56"


def run(*args: str) -> None:
    print("+", " ".join(args), flush=True)
    subprocess.run([sys.executable, *args], cwd=ROOT, check=True)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst")
    ap.add_argument("--verify")
    a = ap.parse_args()
    if a.verify:
        run(str(HERE / "w7_assemble.py"), "--verify", str(ROOT / "port" / "build" / a.verify), str(ROOT / "port" / "build" / "I22"))
        return
    assert a.dst and a.dst != "I22", a.dst
    m = ROOT / "port" / "build" / a.dst
    run(str(HERE / "w7_assemble.py"), "--dst", a.dst, "--commit", BASE, "--plain")
    run(str(HERE / "28_assemble.py"), "--root", str(m), "--only-28")
    run(str(HERE / "29_assemble.py"), "--root", str(m), "--only-29")
    # I22's own edit (decision G21, 2026-11-01): test_app_present.py GROWTH_LIMIT 4 MB -> 8 MB; copied from I22's mirror (read only)
    rel = pathlib.Path("port/test/test_app_present.py")
    src = ROOT / "port" / "build" / "I22" / rel
    if src.exists() and b"decision G21" in src.read_bytes():
        (m / rel).write_bytes(src.read_bytes())
        print("copied", rel, "from I22 (G21)")
    (m / "HEAD_COMMIT").write_text(BASE + " + S3 wave 9 rows 4-5 (= I22's port tree)\n")


if __name__ == "__main__":
    main()
