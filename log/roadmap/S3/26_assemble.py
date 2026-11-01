"""S3.26: the tree at f2dff003 + 22_edits.py (22_assemble.py) + 26_edits.py (the 2D residue fixes).

    ~/bin/py -3.12 log/roadmap/S3/26_assemble.py [--dst S3_26b] [--commit f2dff003]     # a fresh tree through 22_assemble, then 26
    ~/bin/py -3.12 log/roadmap/S3/26_assemble.py --root <dir> --only-26 [--dry]         # an existing tree that holds 22's edits

Every edit is anchored (old text exactly once; an edit whose new text is already there is refused)."""
import argparse
import importlib.util
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s3_11_assemble", HERE / "11_assemble.py")
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)


def apply26(dst: pathlib.Path, dry: bool = False):
    env = A.env_of(HERE / "26_edits.py")
    R = A.Root(dst, dry)
    A.edits(R, env["EDITS"], "s326 26_edits")
    for rel, text in env.get("NEW_FILES", {}).items():
        p = dst / rel
        if p.exists():
            sys.exit(f"new file already exists: {rel}")
        if not dry:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding="utf-8", newline="\n")
        print("new", rel)
    R.flush()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_26b")
    ap.add_argument("--commit", default="f2dff003")
    ap.add_argument("--root")
    ap.add_argument("--only-26", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root and a.only_26:
        dst = pathlib.Path(a.root).resolve()
    elif a.root:
        sys.exit("--root needs --only-26")
    else:
        subprocess.run([sys.executable, str(HERE / "22_assemble.py"), "--dst", a.dst, "--commit", a.commit], check=True)
        dst = A.ROOT / "port" / "build" / a.dst
    apply26(dst, a.dry)
    print("done", dst)


if __name__ == "__main__":
    main()
