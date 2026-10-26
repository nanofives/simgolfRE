"""Roadmap S2.10 (log/roadmap/S2/10_notes.md): the frames of the character editor, the skill-point dialog and the buy-land screen
against the original's back buffer (port/test/framegate.py, one boot per scenario), and the negative controls of this step's
drawing.

The editor's figure needs the in-game families of roadmap S3: S3.6's interface-art cuts and Surface_drawScaled (family `hud`) and
S3.5's composeGolferSprite palette layers, Image_draw with a palette object and the generic addSprite body (family `scene`). In a
tree without them the editor checks are skipped with that reason; with them every editor checkpoint is 0 px. What is left on
buy_land is the hole's OpenGL line (lineTo -> drawMapLine -> Terrain::drawLine, roadmap S3.8): the check is that no differing
pixel lies outside the box of that line. The skill dialog and the help window over the course are compared inside their own
rectangles (the course around them is roadmap S3's).
"""
from __future__ import annotations

import csv
import os
import pathlib
import subprocess
import sys

import numpy as np
import pytest

from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
REFS = ROOT / "log" / "c3" / "alt" / "refs"
pytestmark = pytest.mark.port

EDITOR = [("cov_r_s2_screens", "editor"), ("cov_a_keys3", "q_editor"), ("cov_a_golfer", "button_0"),
          ("cov_a_career", "career_editor")]
# the hole of cov_r_s2_screens on the buy-land map: the pixels that differ on the tree with S3.5 / S3.6 (measured,
# log/roadmap/S2/10_notes.md) lie in x 0x177..0x1ad, y 0x17c..0x180, the tee-to-green line; its box with a 3-pixel margin
BUY_LINE_BOX = (0x174, 0x179, 0x1b1, 0x184)
# the skill dialog: drawFrameR3 at (xbase + 0x2e, 0x32, 0x140, 0x13c) with xbase 0xc8, as (x0, y0, x1, y1), exclusive ends. The
# help window under it (drawWindow's drawFrameR3 at (0xc0, 0x16a, 0x1a0, ...)) is a shaded panel over the course: the course's
# pixels show through it, so it is not compared here (3 px of it differ on the tree with S3.5 / S3.6, all course pixels)
SKILL_RECTS = [(0xf6, 0x32, 0xf6 + 0x140, 0x32 + 0x13c)]


def _families(lib):
    return {n for n in lib.functions() if n.startswith("gfx.family.")}


def _need_s3(lib):
    f = _families(lib)
    if not {"gfx.family.hud", "gfx.family.scene"} <= f:
        pytest.skip("the in-game families of roadmap S3 (hud, scene) are not in this tree: the editor's figure is theirs")


def _need_refs(scen, chk):
    if not (REFS / scen / "det_b" / f"{chk}.png").exists():
        pytest.skip(f"no reference frame {scen}/{chk}")


def _framegate(tmp, scen, chks, negctl=None):
    out = tmp / f"{scen}.tsv"
    imgs = tmp / "img"
    cmd = [sys.executable, "-m", "port.test.framegate", scen, "--checkpoints", ",".join(chks), "--no-probe", "--out", str(out),
           "--images", str(imgs)]
    if negctl:
        cmd += ["--negctl", negctl]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ROOT), env=dict(os.environ))
    assert out.exists(), r.stderr[-2000:]
    rows = {row["checkpoint"]: row for row in csv.DictReader(open(out, encoding="utf-8"), delimiter="\t")}
    return rows, imgs


def _diff_mask(imgs, scen, chk):
    from PIL import Image
    d = np.asarray(Image.open(imgs / f"{scen}.{chk}.diff.png").convert("L"))
    return d > 0


@pytest.mark.parametrize("scen,chk", EDITOR)
def test_editor_checkpoint_is_zero_px(portlib, tmp_path, scen, chk):
    _need_s3(portlib)
    _need_refs(scen, chk)
    rows, _ = _framegate(tmp_path, scen, [chk])
    assert rows[chk]["total_px"] == "0", f"{scen}/{chk}: {rows[chk]['status']} {rows[chk]['total_px']} px ({rows[chk]['detail']})"


def test_buy_land_differs_only_in_the_hole_line(portlib, tmp_path):
    _need_refs("cov_r_s2_screens", "buy_land")
    if "gfx.family.scene" not in _families(portlib):
        pytest.skip("the tee and flag sprites are drawn by the sprite queue's addSprite body (roadmap S3.5)")
    rows, imgs = _framegate(tmp_path, "cov_r_s2_screens", ["buy_land"])
    m = _diff_mask(imgs, "cov_r_s2_screens", "buy_land")
    x0, y0, x1, y1 = BUY_LINE_BOX
    outside = m.copy()
    outside[y0:y1, x0:x1] = False
    assert int(m.sum()) > 0 or rows["buy_land"]["total_px"] == "0"
    assert int(outside.sum()) == 0, f"{int(outside.sum())} px differ outside the hole line's box"


def test_skill_dialog_is_zero_px(portlib, tmp_path):
    _need_s3(portlib)
    rows, imgs = _framegate(tmp_path, "cov_r_s2_screens", ["skill_points", "skill_popup"])
    for chk in ("skill_points", "skill_popup"):
        m = _diff_mask(imgs, "cov_r_s2_screens", chk)
        inside = sum(int(m[y0:y1, x0:x1].sum()) for x0, y0, x1, y1 in SKILL_RECTS)
        assert inside == 0, f"{chk}: {inside} px differ inside the dialog"


# the controls of this step's drawing (sg_negctl_set names), each read against the original's frame of its checkpoint: RED when
# the control's frame differs in more pixels than the correct one's
CONTROLS = [
    ("gfx.family.chareditor", 1, "cov_r_s2_screens", "editor", True),
    ("gfx.family.chareditor", 2, "cov_r_s2_screens", "editor", True),
    ("gfx.family.chareditor", 3, "cov_a_keys3", "q_editor", True),
    ("gfx.family.buyland", 1, "cov_r_s2_screens", "buy_land", False),
    ("gfx.family.buyland", 2, "cov_r_s2_screens", "buy_land", False),
    ("gfx.family.buyland", 3, "cov_r_s2_screens", "buy_land", False),
    ("gfx.attach", 7, "cov_r_s2_screens", "buy_land", False),
]


@pytest.mark.parametrize("name,mode,scen,chk,s3", CONTROLS)
def test_drawing_control_reads_red(portlib, tmp_path, name, mode, scen, chk, s3):
    if s3:
        _need_s3(portlib)
    _need_refs(scen, chk)
    base, _ = _framegate(tmp_path / "base", scen, [chk])
    ctl, _ = _framegate(tmp_path / "ctl", scen, [chk], negctl=f"{name}={mode}")
    assert int(ctl[chk]["total_px"]) > int(base[chk]["total_px"]), f"{name}={mode} blind on {scen}/{chk}"
