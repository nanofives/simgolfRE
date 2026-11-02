"""S3.28: the wave 8 stack (w8_assemble.py) + 28_edits.py (lowerEdgeCorner's draw half as terrain pass kind 5).

    ~/bin/py -3.12 log/roadmap/S3/28_assemble.py [--dst S3_28b] [--commit 2291a342]   # a fresh tree through w8_assemble, then 28
    ~/bin/py -3.12 log/roadmap/S3/28_assemble.py --root <dir> --only-28 [--dry]        # an existing tree that holds the wave 8 stack

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


def apply28(dst: pathlib.Path, dry: bool = False):
    env = A.env_of(HERE / "28_edits.py")
    R = A.Root(dst, dry)
    A.edits(R, env["EDITS"], "s328 28_edits")
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
    ap.add_argument("--dst", default="S3_28b")
    ap.add_argument("--commit", default="2291a342")
    ap.add_argument("--root")
    ap.add_argument("--only-28", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root and a.only_28:
        dst = pathlib.Path(a.root).resolve()
    elif a.root:
        sys.exit("--root needs --only-28")
    else:
        subprocess.run([sys.executable, str(HERE / "w8_assemble.py"), "--dst", a.dst, "--commit", a.commit], check=True)
        dst = A.ROOT / "port" / "build" / a.dst
    apply28(dst, a.dry)
    print("done", dst)


if __name__ == "__main__":
    main()
