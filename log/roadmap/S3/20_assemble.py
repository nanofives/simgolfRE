"""S3.20: the S3 wave 7 tree with the host-OpenGL default and its startup self-check = w7_assemble.py's tree (git archive 37e58e96 +
the wave 7 steps, S3.9's port/build/S3_9) + 20_edits.py (EDITS, NEW_FILES).

    ~/bin/py -3.12 log/roadmap/S3/20_assemble.py [--dst S3_20] [--commit 37e58e96]       # a fresh tree (runs w7_assemble.py first)
    ~/bin/py -3.12 log/roadmap/S3/20_assemble.py --root <dir> --only-20                  # an existing tree that already holds wave 7
    ~/bin/py -3.12 log/roadmap/S3/20_assemble.py ... --dry

Every edit is anchored (old text exactly once; an edit whose new text is already there is refused); a new file must not exist.
Builds are separate (port/test/build.py). No shared file is edited (module gl_wgl is listed in modules.inc by S3.15)."""
import argparse
import importlib.util
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s3_11_assemble", HERE / "11_assemble.py")
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)


def apply20(dst: pathlib.Path, dry: bool = False):
    env = A.env_of(HERE / "20_edits.py")
    R = A.Root(dst, dry)
    A.edits(R, env["EDITS"], "s320 20_edits")
    R.flush()
    for rel, text in env["NEW_FILES"].items():
        p = dst / rel
        assert not p.exists(), f"{rel} exists already"
        if not dry:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding="utf-8", newline="\n")
        print("new", rel)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_20")
    ap.add_argument("--commit", default="37e58e96")
    ap.add_argument("--root")
    ap.add_argument("--only-20", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root and a.only_20:
        dst = pathlib.Path(a.root).resolve()
    elif a.root:
        sys.exit("--root needs --only-20 (run w7_assemble.py on the tree first)")
    else:
        subprocess.run([sys.executable, str(HERE / "w7_assemble.py"), "--dst", a.dst, "--commit", a.commit], check=True)
        dst = A.ROOT / "port" / "build" / a.dst
    apply20(dst, a.dry)
    print("done", dst)


if __name__ == "__main__":
    main()
