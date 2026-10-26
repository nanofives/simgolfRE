"""S2.10: a MEASUREMENT tree port/build/S2_10_s3 = this step's mirror (port/build/S2_10_head) + the in-flight S3.5 / S3.6 work
(port/build/S3_5_root: S3.6's base with S3.5's files), so the character editor's sprite cuts (loadInterfaceArt's whole image
half, S3.6), its scaled draws (Surface_drawScaled, S3.6) and the golfer palette (composeGolferSprite's layers, S3.5) can be
seen. Not a deliverable: the merge request (10_merge.md) is against S2_13_head alone.

Every file under port/src, port/include and port/test that S3_5_root changed against port/build/S2_4_head (the common base of
both lineages) is 3-way merged (`git merge-file`, base = S2_4_head) into a copy of S2_10_head; new files are copied. Prints one
line per file (new / clean / conflict).

    ~/bin/py -3.12 log/roadmap/S2/10_tools/make_s3_tree.py
"""
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
B = ROOT / "port" / "build"
OURS, BASE, THEIRS, OUT = B / "S2_10_head", B / "S2_4_head", B / "S3_5_root", B / "S2_10_s3"
SUBS = ["port/src", "port/include", "port/test"]
JUNCTIONS = ["log", "original", "port/corpus", "re/replay", "re/versions", "shim/build", "tests/fixtures", "tests/golden"]


def lf(p):
    return p.read_bytes().replace(b"\r\n", b"\n")


def walk(root, sub):
    out = {}
    base = root / sub
    if not base.exists():
        return out
    for p in base.rglob("*"):
        if p.is_file() and "__pycache__" not in p.parts and not str(p.relative_to(root)).startswith("port/build"):
            out[str(p.relative_to(root)).replace("\\", "/")] = p
    return out


def main():
    if OUT.exists():
        print(f"{OUT} exists: remove it first (its junctions with `cmd /c rmdir`)")
        return 1
    r = subprocess.run(["robocopy", str(OURS), str(OUT), "/E", "/XJ", "/XD", str(OURS / "port" / "build"), "__pycache__",
                        *[str(OURS / j) for j in JUNCTIONS], "/NFL", "/NDL", "/NJH", "/NJS", "/NP"], capture_output=True)
    if r.returncode >= 8:
        print("robocopy failed", r.returncode)
        return 1
    for j in JUNCTIONS:
        subprocess.run(["cmd", "/c", "mklink", "/J", str(OUT / j), str(ROOT / j)], capture_output=True)
    tmp = B / "S2_10_s3_tmp"
    tmp.mkdir(exist_ok=True)
    conflicts = 0
    for sub in SUBS:
        theirs, base = walk(THEIRS, sub), walk(BASE, sub)
        for rel, tp in sorted(theirs.items()):
            op = OUT / rel
            if rel not in base:
                if op.exists() and lf(op) != lf(tp):
                    print(f"both new, differ (kept ours): {rel}")
                    continue
                op.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(tp, op)
                print(f"new: {rel}")
                continue
            if lf(tp) == lf(base[rel]):
                continue
            if not op.exists():
                print(f"theirs changed, ours deleted: {rel}")
                continue
            o, b, t = lf(op), lf(base[rel]), lf(tp)
            if o == b:
                op.write_bytes(t)
                print(f"clean(fast): {rel}")
                continue
            (tmp / "o").write_bytes(o)
            (tmp / "b").write_bytes(b)
            (tmp / "t").write_bytes(t)
            rr = subprocess.run(["git", "merge-file", "-L", "ours", "-L", "base", "-L", "s3", str(tmp / "o"), str(tmp / "b"),
                                 str(tmp / "t")], capture_output=True)
            op.write_bytes((tmp / "o").read_bytes())
            if rr.returncode == 0:
                print(f"clean: {rel}")
            else:
                conflicts += 1
                print(f"CONFLICT ({rr.returncode}): {rel}")
    print(f"conflicts: {conflicts}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
