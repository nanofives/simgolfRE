"""S11.1: dry-run of the four promote commands with merge section 3 (A) applied to a scratch copy of UNCERTAINTIES.md.
Read-only: re_classify's cmd_check with write=False; the scratch file lives in the scratchpad given as argv[1].
Run: ~/bin/py -3.12 log/roadmap/S11/1_check_after.py <scratch dir>
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "scripts"))
import re_classify as rc  # noqa: E402


def main() -> int:
    scratch = pathlib.Path(sys.argv[1])
    scratch.mkdir(parents=True, exist_ok=True)
    text = (ROOT / "UNCERTAINTIES.md").read_text()
    lines = text.splitlines(keepends=True)
    n = 0
    for i, l in enumerate(lines):
        if l.startswith("| U-1351 |"):
            lines[i] = re.sub(r"\|\s*open\s*\|\s*$", "| resolved |", l.rstrip("\n")) + "\n"
            n += 1
    assert n == 1, n
    u = scratch / "UNCERTAINTIES.md"
    u.write_text("".join(lines))
    p = rc.Project(ROOT)
    p.uncert = u
    rows = [("0x0045baf0", None, "log/diff/0045baf0__WinMain_16.match.csv"),
            ("0x00493000", None, "log/diff/00493000__scan_out_line_poly_16__YAXHH_Z.match.csv"),
            ("0x00493080", None, "log/diff/00493080__scan_out_line_poly_8__YAXHH_Z.match.csv"),
            ("0x0004a010", "sound.dll", "log/roadmap/S11/1_strcpy_lib.csv")]
    rc_all = 0
    for addr, mod, ev in rows:
        a = argparse.Namespace(addr=addr, to="C4", name=None, subsystem=None, note=None, file=None, callers=None,
                               callees=None, module=mod, evidence=ev)
        rc_all |= rc.cmd_check(p, a, write=False)
    return rc_all


if __name__ == "__main__":
    sys.exit(main())
