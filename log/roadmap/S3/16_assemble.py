"""S3.16: the wave 7 step tree = 12_assemble.py's tree (git archive ee6efc1d + wave 6) + S3.16's anchored edits (16_edits.py).

    ~/bin/py -3.12 log/roadmap/S3/16_assemble.py [--dst S3_16] [--commit ee6efc1d] [--root <dir>]

16_edits.py = the drawing edits, 16_snapshot_edits.py = the gate harness's checkpoint position (gfx.py, framegate.py);
--only drawing|snapshot applies one of them. --root <dir> applies only S3.16's edits to an existing tree (e.g. the integrator's tree after its own assembly). Every edit is
anchored: its old text must occur exactly once, and an edit whose new text is already there is refused (applied twice)."""
import argparse
import pathlib
import runpy
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def apply(dst: pathlib.Path, files=("16_edits.py", "16_snapshot_edits.py")):
    edits = [e for f in files for e in runpy.run_path(str(HERE / f))["EDITS"]]
    for rel, old, new in edits:
        p = dst / rel
        t = p.read_text(encoding="utf-8")
        assert t.count(old) == 1, f"{rel}: anchor found {t.count(old)} times: {old[:80]!r}"
        assert new not in t or old in new, f"{rel}: edit already applied: {new[:80]!r}"
        p.write_text(t.replace(old, new), encoding="utf-8")
        print("edit", rel, len(old), "->", len(new))
    if "16_edits.py" in files:   # the files S3.16 adds (16_make_edits.py NEW_FILES)
        import shutil
        for f in sorted((HERE / "16_new").rglob("*")):
            if f.is_file():
                rel = f.relative_to(HERE / "16_new")
                assert not (dst / rel).exists(), f"{rel} already exists"
                shutil.copyfile(f, dst / rel)
                print("new", rel)
    # MSBuild does not rebuild registry.cpp when only .inc files change
    (dst / "port/src/registry.cpp").touch()
    print("applied", len(edits), "edits to", dst)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_16")
    ap.add_argument("--commit", default="ee6efc1d")
    ap.add_argument("--root")
    ap.add_argument("--only", choices=["drawing", "snapshot"], help="apply one of the two edit files (default both)")
    a = ap.parse_args()
    files = {"drawing": ("16_edits.py",), "snapshot": ("16_snapshot_edits.py",)}.get(a.only, ("16_edits.py", "16_snapshot_edits.py"))
    if a.root:
        apply(pathlib.Path(a.root).resolve(), files)
        return
    subprocess.run([sys.executable, str(HERE / "12_assemble.py"), "--dst", a.dst, "--commit", a.commit], check=True)
    apply(ROOT / "port" / "build" / a.dst, files)


if __name__ == "__main__":
    main()
