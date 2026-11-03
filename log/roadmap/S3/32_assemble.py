"""S3.32: the wave 10 base (w10_base.py) [+ the S3.31 / S3.30 layers] + 32_edits.py (the boot's static Surface vtable words;
log/roadmap/S3/32_notes.md).

    ~/bin/py -3.12 log/roadmap/S3/32_assemble.py [--dst S3_32v]                     # a fresh w10_base tree, then the edits
    ~/bin/py -3.12 log/roadmap/S3/32_assemble.py --root <dir> --only-32 [--dry]     # an existing tree that holds the wave 10 base

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


def apply32(dst: pathlib.Path, dry: bool = False):
    env = A.env_of(HERE / "32_edits.py")
    R = A.Root(dst, dry)
    A.edits(R, env["EDITS"], "s332 32_edits")
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
    ap.add_argument("--dst", default="S3_32v")
    ap.add_argument("--root")
    ap.add_argument("--only-32", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root and a.only_32:
        dst = pathlib.Path(a.root).resolve()
    elif a.root:
        sys.exit("--root needs --only-32")
    else:
        subprocess.run([sys.executable, str(HERE / "w10_base.py"), "--dst", a.dst], check=True)
        dst = A.ROOT / "port" / "build" / a.dst
    apply32(dst, a.dry)
    print("done", dst)


if __name__ == "__main__":
    main()
