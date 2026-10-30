"""S3.17: the wave 6 tree (12_assemble.py on ee6efc1d = port/build/I15) + S3.17's change: the routing map's tee / flag sprites
through addSprite (routing.cpp), the Surface_createFull_fwd1 body (gfx_surface.cpp), the boot last frame's tail idle passes
(tick.cpp), test_gfx_routing.py's bound, the new port/test/test_gfx_s3_17.py, the UNCERTAINTIES.md rows U-1830..U-1833
(17_uncertain.tsv) and the CLAUDE.md paragraph of 17_merge.md section 4 (after S3.12's paragraph).

    ~/bin/py -3.12 log/roadmap/S3/17_assemble.py [--dst S3_17v] [--commit ee6efc1d] [--recopy] [--no-docs]
    ~/bin/py -3.12 log/roadmap/S3/17_assemble.py --root <dir> --only [--no-docs]   # S3.17 alone on a tree that holds wave 6

Without --only the wave 6 tree is made first by 12_assemble.py with the same --dst / --commit / --recopy / --root / --no-docs.
Every edit is anchored (its old text exactly once; an edit whose new text is already there is refused). A new file is refused
when the root already holds a different one. Log: 17_assemble*.log."""
import argparse
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import importlib.util  # noqa: E402

spec = importlib.util.spec_from_file_location("s3_11_assemble", HERE / "11_assemble.py")
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)

NEW = HERE / "17_new"
NEW_FILES = ["port/test/test_gfx_s3_17.py"]
CLAUDE_ANCHOR = "about 1 run of 6 without workers too (U-1399)."


TICK = "port/src/sim/tick.cpp"   # 17_merge.md section 3: its own merge item (the boot last frame's tail idle passes)


def s317(R, docs=True, tick=True):
    ed = [e for e in A.env_of(HERE / "17_edits.py")["EDITS"] if tick or e[0] != TICK]
    A.edits(R, ed, "s317 17_edits" + ("" if tick else " (without tick.cpp)"))
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
    R.unc_rows(A.I6.rows_from_tsv(HERE / "17_uncertain.tsv"), "s317 UNCERTAINTIES U-1830..U-1833")
    para = A.fenced_after((HERE / "17_merge.md").read_text(encoding="utf-8"), "## 4. CLAUDE.md paragraph")
    nl = R.nl("CLAUDE.md")
    R.replace("CLAUDE.md", CLAUDE_ANCHOR, CLAUDE_ANCHOR + nl + nl.join(para.strip("\n").split("\n")), "s317 CLAUDE.md")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_17v")
    ap.add_argument("--commit", default="ee6efc1d")
    ap.add_argument("--recopy", action="store_true")
    ap.add_argument("--root")
    ap.add_argument("--only", action="store_true", help="with --root: apply S3.17 alone (the root already holds wave 6)")
    ap.add_argument("--no-docs", action="store_true")
    ap.add_argument("--no-tick", action="store_true", help="leave out the tick.cpp edit (17_merge.md section 3)")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.only and not a.root:
        ap.error("--only needs --root")
    if not a.only:
        cmd = [sys.executable, str(HERE / "12_assemble.py"), "--commit", a.commit]
        cmd += ["--root", a.root] if a.root else ["--dst", a.dst]
        cmd += (["--recopy"] if a.recopy else []) + (["--no-docs"] if a.no_docs else []) + (["--dry"] if a.dry else [])
        subprocess.run(cmd, check=True)
    dst = pathlib.Path(a.root).resolve() if a.root else A.ROOT / "port" / "build" / a.dst
    R = A.Root(dst, a.dry)
    s317(R, docs=not a.no_docs, tick=not a.no_tick)
    R.flush()
    (dst / "port" / "src" / "registry.cpp").touch()
    print("done", dst)


if __name__ == "__main__":
    main()
