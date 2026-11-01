"""Wave 8 fix layer for I21's suite (2026-10-31): two test pins wave 8 moved. Applied after w8_assemble.py (and so under wave 9's
stack too). Every old text must occur exactly once; an edit already present is refused.

    ~/bin/py -3.12 log/roadmap/S3/w8_fix_i21.py --root <tree> [--dry]

1. port/test/test_gfx_pixel.py test_family_registered: S3.26 gave family `pixel` the Widget_readPixel body (c3db.cpp GetPixel_re),
   so the family holds 2 bodies with `reports` present (Widget_writePixel, Widget_readPixel) and 3 without it (+ Widget_fillRect).
2. port/test/test_gfx_reports.py RESIDUAL: S3.26 draws the hire dialog's four animated employees from
   cellAt(idx + 0x20e, frame mod cols, s) (commands_screens.inc show_hire_employee_dialog), so cov_r_s2_screens `hire` has no judged
   pixel left that differs (was 1,397 in the employees' box); the judged count 148,198 is unchanged.
"""
import argparse
import pathlib
import sys

EDITS = [
    ("port/test/test_gfx_pixel.py",
     '        assert st.call("gfx.family.pixel", []).eax == (1 if plib.has("gfx.family.reports") else 2)\n',
     '        # roadmap S3.26 added the Widget_readPixel body to this family (Widget_fillRect is the reports family\'s when it is linked)\n'
     '        assert st.call("gfx.family.pixel", []).eax == (2 if plib.has("gfx.family.reports") else 3)\n'),
    ("port/test/test_gfx_reports.py",
     '# judged pixels that still differ, all of them the hire dialog\'s four animated employees: addSprite draws a FLIC sheet cell\n'
     '# through drawQueuedSprite (roadmap S3\'s sprites; reported unsupported, U-1272): checkpoint -> (count, bbox x0, y0, x1, y1)\n'
     'RESIDUAL = {"hire": (1397, (556, 167, 586, 403))}\n',
     '# judged pixels that still differ: checkpoint -> (count, bbox x0, y0, x1, y1). Empty since roadmap S3.26: the hire dialog\'s four\n'
     '# animated employees (1,397 px in (556, 167, 586, 403) before) are drawn from cellAt(idx + 0x20e, frame mod cols, s)\n'
     '# (commands_screens.inc show_hire_employee_dialog), so every judged pixel of `hire` equals the original\'s.\n'
     'RESIDUAL = {}\n'),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--dry", action="store_true")
    a = ap.parse_args()
    root = pathlib.Path(a.root)
    out = {}
    for rel, old, new in EDITS:
        p = root / rel
        s = out.get(rel) or p.read_bytes().decode("utf-8")
        crlf = "\r\n" in s
        o, n = (old.replace("\n", "\r\n"), new.replace("\n", "\r\n")) if crlf else (old, new)
        if s.count(n) == 1 and s.count(o) == 0:
            sys.exit(f"refused: {rel}: edit already applied")
        k = s.count(o)
        if k != 1:
            sys.exit(f"refused: {rel}: old text found {k} times")
        out[rel] = s.replace(o, n)
        print(f"edit {rel}")
    if a.dry:
        print("dry run: nothing written")
        return
    for rel, s in out.items():
        (root / rel).write_bytes(s.encode("utf-8"))
        print(f"wrote {rel}")


if __name__ == "__main__":
    main()
