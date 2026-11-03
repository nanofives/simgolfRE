"""S3.30: the wave 10 base (w10_base.py) + 30_edits.py (the trace's DELIVER kind 7 `call`; log/roadmap/S3/30_notes.md).

    ~/bin/py -3.12 log/roadmap/S3/30_assemble.py [--dst S3_30v]                     # a fresh w10_base tree, then the edits
    ~/bin/py -3.12 log/roadmap/S3/30_assemble.py --root <dir> --only-30 [--dry]     # an existing tree that holds the wave 10 base

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


def apply30(dst: pathlib.Path, dry: bool = False):
    env = A.env_of(HERE / "30_edits.py")
    R = A.Root(dst, dry)
    A.edits(R, env["EDITS"], "s330 30_edits")
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
    ap.add_argument("--dst", default="S3_30v")
    ap.add_argument("--root")
    ap.add_argument("--only-30", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root and a.only_30:
        dst = pathlib.Path(a.root).resolve()
    elif a.root:
        sys.exit("--root needs --only-30")
    else:
        subprocess.run([sys.executable, str(HERE / "w10_base.py"), "--dst", a.dst], check=True)
        dst = A.ROOT / "port" / "build" / a.dst
    apply30(dst, a.dry)
    print("done", dst)


if __name__ == "__main__":
    main()
