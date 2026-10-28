"""S3.13: the S3 resubmission tree on 78ffb06d = log/roadmap/S3/11_assemble.py's steps (waves 2 / 4 / 5, S3.7, S3.8, S3.11) +
S3.13's edits (13_edits.py: one body per name for drawStringRightAt and drawWindow, S2.11's duplicate ticker draw removed, the
moved controls, the integrator.py evidence entry for flic.treeAliasNames) + the S3.13 CLAUDE.md paragraph (MERGE_w5b.md section 6).

    ~/bin/py -3.12 log/roadmap/S3/13_assemble.py [--dst S3_13] [--commit 78ffb06d] [--recopy] [--root <dir>]

Same arguments as 11_assemble.py (its main() runs with step s313 appended). Log: 13_assemble*.log."""
import importlib.util
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s3_11_assemble", HERE / "11_assemble.py")
A = importlib.util.module_from_spec(spec)
spec.loader.exec_module(A)


def s313(R):
    """S3.13's edits (13_edits.py) and the CLAUDE.md paragraph of MERGE_w5b.md section 6"""
    A.edits(R, A.env_of(HERE / "13_edits.py")["EDITS"], "s313 13_edits")
    para = A.fenced_after((HERE / "MERGE_w5b.md").read_text(encoding="utf-8"), "## 6. CLAUDE.md paragraph")
    A.claude_before(R, "Phase 7 defect decisions, section S6 of the roadmap orchestrator", para, "s313 CLAUDE.md")


A.STEPS.append(("s313", s313))
A.OWN["s313"] = []

if __name__ == "__main__":
    if not any(x.startswith("--dst") or x == "--root" for x in sys.argv[1:]):
        sys.argv[1:1] = ["--dst", "S3_13"]
    if not any(x.startswith("--commit") for x in sys.argv[1:]):
        sys.argv[1:1] = ["--commit", "78ffb06d"]
    A.main()
