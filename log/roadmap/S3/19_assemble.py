"""S3.19: the wave 7 tree up to S3.18 (18_assemble.py: wave 6 + S3.17 + S3.18) + S3.19's change: the bio EditBox in the graphics
backend (new body family gfx_editbox.cpp and its gfx_families.inc line; gfx_text.cpp's window target and measureMarkupRun;
gfx.cpp's window-tree composition at Window::refreshFull; chareditor.cpp's bio entry hands the EditBox to the backend), the new
port/test/test_gfx_s3_19.py, the UNCERTAINTIES.md rows U-1846.. (19_uncertain.tsv) and the CLAUDE.md paragraph of 19_merge.md
section 4 (after S3.18's paragraph).

    ~/bin/py -3.12 log/roadmap/S3/19_assemble.py [--dst S3_19v] [--commit ee6efc1d] [--recopy] [--no-docs]
    ~/bin/py -3.12 log/roadmap/S3/19_assemble.py --root <dir> --only [--no-docs]   # S3.19 alone on a tree that holds S3.18

Without --only the S3.18 tree is made first by 18_assemble.py with the same --dst / --commit / --recopy / --root / --no-docs.
Every edit is anchored (its old text exactly once; an edit whose new text is already there is refused). A new file is refused
when the root already holds a different one."""
import argparse
import importlib.util
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

spec = importlib.util.spec_from_file_location("s3_11_assemble", HERE / "11_assemble.py")
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)

NEW = HERE / "19_new"
NEW_FILES = ["port/src/sim/gfx_editbox.cpp", "port/test/test_gfx_s3_19.py"]
CLAUDE_ANCHOR = "UNSUPPORTED only through framegate's cumulative reason list (U-1845)."


def s319(R, docs=True):
    A.edits(R, A.env_of(HERE / "19_edits.py")["EDITS"], "s319 19_edits")
    for rel in NEW_FILES:   # written directly (the Root helper's flush compares with the file on disk)
        src = (NEW / rel).read_bytes()
        p = R.root / rel
        if p.exists():
            assert p.read_bytes() == src, f"{rel}: the root holds a different file"
        elif not R.dry:
            p.write_bytes(src)
        print("new", rel)
    if not docs:
        return
    R.unc_rows(A.I6.rows_from_tsv(HERE / "19_uncertain.tsv"), "s319 UNCERTAINTIES U-1846..")
    para = A.fenced_after((HERE / "19_merge.md").read_text(encoding="utf-8"), "## 4. CLAUDE.md paragraph")
    nl = R.nl("CLAUDE.md")
    R.replace("CLAUDE.md", CLAUDE_ANCHOR, CLAUDE_ANCHOR + nl + nl.join(para.strip("\n").split("\n")), "s319 CLAUDE.md")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_19v")
    ap.add_argument("--commit", default="ee6efc1d")
    ap.add_argument("--recopy", action="store_true")
    ap.add_argument("--root")
    ap.add_argument("--only", action="store_true", help="with --root: apply S3.19 alone (the root already holds S3.18)")
    ap.add_argument("--no-docs", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.only and not a.root:
        ap.error("--only needs --root")
    if not a.only:
        cmd = [sys.executable, str(HERE / "18_assemble.py"), "--commit", a.commit]
        cmd += ["--root", a.root] if a.root else ["--dst", a.dst]
        cmd += (["--recopy"] if a.recopy else []) + (["--no-docs"] if a.no_docs else []) + (["--dry"] if a.dry else [])
        subprocess.run(cmd, check=True)
    dst = pathlib.Path(a.root).resolve() if a.root else A.ROOT / "port" / "build" / a.dst
    R = A.Root(dst, a.dry)
    s319(R, docs=not a.no_docs)
    R.flush()
    (dst / "port" / "src" / "registry.cpp").touch()
    print("done", dst)


if __name__ == "__main__":
    main()
