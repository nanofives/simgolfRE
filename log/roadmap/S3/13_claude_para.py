"""S3.13: put MERGE_w5b.md section 6's CLAUDE.md paragraph into an assembled tree without re-assembling it (13_assemble.py's step s313
writes the same paragraph, but the measured tree port/build/S3_13 was assembled before section 6 was written).

    ~/bin/py -3.12 log/roadmap/S3/13_claude_para.py [--root port/build/S3_13]

Refuses when the paragraph's first line is already in CLAUDE.md (claude_before)."""
import argparse
import importlib.util
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[2]
spec = importlib.util.spec_from_file_location("s3_13_assemble", HERE / "13_assemble.py")
M = importlib.util.module_from_spec(spec)
spec.loader.exec_module(M)
A = M.A


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=str(REPO / "port" / "build" / "S3_13"))
    a = ap.parse_args()
    para = A.fenced_after((HERE / "MERGE_w5b.md").read_text(encoding="utf-8"), "## 6. CLAUDE.md paragraph")
    assert "CLAUDE_PARA" not in para, "section 6 still holds the placeholder"
    R = A.Root(pathlib.Path(a.root), False)
    A.claude_before(R, "Phase 7 defect decisions, section S6 of the roadmap orchestrator", para, "s313 CLAUDE.md")
    R.flush()
    print("done", a.root)


if __name__ == "__main__":
    main()
