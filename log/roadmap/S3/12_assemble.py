"""S3.12: the wave 6 tree = `git archive e179bd24` + the junctions of 11_assemble.py + 12_edits.py (S3.10's rasteriser workers,
S3.10b's terrain_state view, the LiveTerrain log switch with S7's TerrainLogBound, the app-test facts parameter) + the wave 6
documents (MERGE_w6.md: the CLAUDE.md paragraph after S3.14's, and the UNCERTAINTIES.md rows of section 5).

    ~/bin/py -3.12 log/roadmap/S3/12_assemble.py [--dst S3_12] [--commit e179bd24] [--recopy] [--root <dir>] [--no-docs] [--plain]

--root <dir> applies the edits (and the documents) to an existing tree, e.g. `.` for the integrator's tree; --plain makes the
archive tree only (no edit: the plain reference build of the commit); --no-docs skips CLAUDE.md / UNCERTAINTIES.md. Every edit is
anchored: its old text must occur exactly once, and an edit whose new text is already there is refused (applied twice).
Builds are separate (port/test/build.py). Log: 12_assemble*.log."""
import argparse
import importlib.util
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s3_11_assemble", HERE / "11_assemble.py")
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)

CLAUDE_ANCHOR = "app files' pacing gates measure the host:\nrun them one file per process at low load."


def docs(R):
    md = (HERE / "MERGE_w6.md").read_text(encoding="utf-8")
    para = A.fenced_after(md, "## 6. CLAUDE.md paragraph")
    nl = R.nl("CLAUDE.md")
    anchor = nl.join(CLAUDE_ANCHOR.split("\n"))
    R.replace("CLAUDE.md", anchor, anchor + nl + nl.join(para.strip("\n").split("\n")), "s312 CLAUDE.md")
    # UNCERTAINTIES.md: rows replaced by id (the row's first cell) and rows appended (MERGE_w6.md section 5)
    sec = A.section(md, "## 5. UNCERTAINTIES.md")
    rep = [l for l in A.fenced_after(sec, "### replace").split("\n") if l.startswith("| U-")]
    add = [l for l in A.fenced_after(sec, "### append").split("\n") if l.startswith("| U-")]
    for row in rep:
        A.replace_unc_row(R, row.split("|")[1].strip(), row, "s312 UNCERTAINTIES " + row.split("|")[1].strip())
    t = R.get("UNCERTAINTIES.md")
    for row in add:
        uid = row.split("|")[1].strip()
        assert f"| {uid} |" not in t, f"{uid} already in UNCERTAINTIES.md"
    R.unc_rows(add, "s312 UNCERTAINTIES append")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_12")
    ap.add_argument("--commit", default="e179bd24")
    ap.add_argument("--recopy", action="store_true")
    ap.add_argument("--root")
    ap.add_argument("--no-docs", action="store_true")
    ap.add_argument("--plain", action="store_true")
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    if a.root:
        dst = pathlib.Path(a.root).resolve()
    else:
        dst = A.ROOT / "port" / "build" / a.dst
        A.make_tree(dst, a.commit, a.recopy)
    if a.plain:
        print("plain tree", dst)
        return
    R = A.Root(dst, a.dry)
    A.edits(R, A.env_of(HERE / "12_edits.py")["EDITS"], "s312 12_edits")
    if not a.no_docs:
        docs(R)
    R.flush()
    (dst / "port" / "src" / "registry.cpp").touch()
    print("done", dst)


if __name__ == "__main__":
    main()
