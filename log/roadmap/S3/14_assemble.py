"""S3.14: the S3 resubmission tree on d66aa755 = log/roadmap/S3/13_assemble.py's steps (11_assemble.py's waves 2 / 4 / 5, S3.7,
S3.8, S3.11, then S3.13's edits) + S3.14's edits (14_edits.py: the last frame's pending gfx snapshots, the app launched in the
Normal priority class, the drivers waiting for the title menu's frame) + the S3.14 addition to S3.13's CLAUDE.md paragraph
(14_merge.md section 3).

    ~/bin/py -3.12 log/roadmap/S3/14_assemble.py [--dst S3_14] [--commit d66aa755] [--recopy] [--root <dir>]

Same arguments as 11_assemble.py (its main() runs with steps s313 and s314 appended). Log: 14_assemble*.log."""
import importlib.util
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("s3_13_assemble", HERE / "13_assemble.py")
B = importlib.util.module_from_spec(spec)
spec.loader.exec_module(B)   # appends s313 to 11_assemble.py's STEPS (B.A)
A = B.A

CLAUDE_ANCHOR = "(13_suite_rest.sh FROM=<file>), and long suites run detached."


def s314(R):
    """S3.14's edits (14_edits.py) and the CLAUDE.md addition after S3.13's paragraph (14_merge.md section 3)"""
    A.edits(R, A.env_of(HERE / "14_edits.py")["EDITS"], "s314 14_edits")
    add = A.fenced_after((HERE / "14_merge.md").read_text(encoding="utf-8"), "## 3. CLAUDE.md addition")
    nl = R.nl("CLAUDE.md")
    add = nl.join(add.strip("\n").split("\n"))
    R.replace("CLAUDE.md", CLAUDE_ANCHOR, CLAUDE_ANCHOR + nl + add, "s314 CLAUDE.md")


A.STEPS.append(("s314", s314))
A.OWN["s314"] = []

if __name__ == "__main__":
    if not any(x.startswith("--dst") or x == "--root" for x in sys.argv[1:]):
        sys.argv[1:1] = ["--dst", "S3_14"]
    if not any(x.startswith("--commit") for x in sys.argv[1:]):
        sys.argv[1:1] = ["--commit", "d66aa755"]
    A.main()
