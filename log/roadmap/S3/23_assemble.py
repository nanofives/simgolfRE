"""S3.23: the tree at f2dff003 (wave 7 committed) + 23_edits.py (framegate --split and test_framegate_split.py).

    ~/bin/py -3.12 log/roadmap/S3/23_assemble.py [--dst S3_23] [--commit f2dff003]     # a fresh plain tree, then the edits
    ~/bin/py -3.12 log/roadmap/S3/23_assemble.py --root <dir> --only-23 [--dry]        # an existing tree at f2dff003

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


def apply23(dst: pathlib.Path, dry: bool = False):
    env = A.env_of(HERE / "23_edits.py")
    R = A.Root(dst, dry)
    A.edits(R, env["EDITS"], "s323 23_edits")
    R.flush()
    for rel, text in env["NEW_FILES"].items():
        p = dst / rel
        assert not p.exists(), f"{rel} exists already"
        if not dry:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding="utf-8", newline="")
        print("new", rel)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_23")
    ap.add_argument("--commit", default="f2dff003")
    ap.add_argument("--root")
    ap.add_argument("--only-23", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root and a.only_23:
        dst = pathlib.Path(a.root).resolve()
    elif a.root:
        sys.exit("--root needs --only-23")
    else:
        subprocess.run([sys.executable, str(HERE / "w7_assemble.py"), "--dst", a.dst, "--commit", a.commit, "--plain"], check=True)
        dst = A.ROOT / "port" / "build" / a.dst
    apply23(dst, a.dry)
    print("done", dst)


if __name__ == "__main__":
    main()
