"""S3.15: the wave 7 tree with the host-OpenGL terrain backend = 12_assemble.py's tree (git archive ee6efc1d + wave 6) + 15_edits.py
(EDITS, SHARED_EDITS = the modules.inc line, NEW_FILES) + 15_framegate_edits.py (framegate's `--host-gl`, kept apart for the
lead's merge with S3.16's framegate edits).

    ~/bin/py -3.12 log/roadmap/S3/15_assemble.py [--dst S3_15] [--commit ee6efc1d] [--recopy] [--no-docs]
    ~/bin/py -3.12 log/roadmap/S3/15_assemble.py --root <dir> [--only-15]     # an existing tree (--only-15: it already holds wave 6)

Every edit is anchored (old text exactly once; an edit whose new text is already there is refused); a new file must not exist.
Builds are separate (port/test/build.py). Log: 15_assemble*.log."""
import argparse
import importlib.util
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s3_12_assemble", HERE / "12_assemble.py")
B = importlib.util.module_from_spec(spec)
spec.loader.exec_module(B)
A = B.A


def apply15(dst: pathlib.Path, dry: bool = False):
    env = A.env_of(HERE / "15_edits.py")
    R = A.Root(dst, dry)
    A.edits(R, env["EDITS"], "s315 15_edits")
    A.edits(R, env["SHARED_EDITS"], "s315 15_edits shared")
    A.edits(R, A.env_of(HERE / "15_framegate_edits.py")["EDITS"], "s315 15_framegate_edits")
    R.flush()
    for rel, text in env["NEW_FILES"].items():
        p = dst / rel
        assert not p.exists(), f"{rel} exists already"
        if not dry:
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(text, encoding="utf-8", newline="")
        print("new", rel)
    (dst / "port" / "src" / "registry.cpp").touch()   # MSBuild does not rebuild it when only modules.inc changes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_15")
    ap.add_argument("--commit", default="ee6efc1d")
    ap.add_argument("--recopy", action="store_true")
    ap.add_argument("--root")
    ap.add_argument("--only-15", action="store_true")
    ap.add_argument("--no-docs", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root and a.only_15:
        dst = pathlib.Path(a.root).resolve()
    else:
        argv = ["12_assemble.py"] + (["--root", a.root] if a.root else ["--dst", a.dst, "--commit", a.commit]) + \
            (["--recopy"] if a.recopy else []) + (["--no-docs"] if a.no_docs else []) + (["--dry"] if a.dry else [])
        sys.argv = argv
        B.main()
        dst = pathlib.Path(a.root).resolve() if a.root else A.ROOT / "port" / "build" / a.dst
    apply15(dst, a.dry)
    print("done", dst)


if __name__ == "__main__":
    main()
