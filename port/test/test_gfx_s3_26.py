"""Roadmap S3.26: the 2D residue of wave 8's groups 7, 6, 4 and 3 (log/roadmap/S3/26_notes.md), judged in the boxes 22_rows.tsv
measured, against the original's back buffer, on framegate's boot (LiveTerrain attached, the default renderer: the boxes hold
2D pixels drawn over the terrain, so they are judged beyond one level, which GlRaster's terrain residue never reaches).

  * group 7, cov_b_inject `zoom_box`: mainLoop P5's ball marker over the zoom box (Surface_drawScaled of g_sprite567b08 at
    0x00418bb8) is drawn only when Widget_readPixel of the magnified point is 0x7fff (0x00418b98). The pixel family's
    Widget_readPixel body hands the root's pixel back to the frame code (gfx_take_value). Control pixel 2 (the value with bit 0
    flipped): the marker is not drawn, 15 px.
  * group 6, cov_r_s2_screens `hire`: the four employees are addSprite of cellAt(idx + 0x20e, frame mod cols, s) (0x00459777),
    the port handed cell 0. Control showHireEmployeeDialog 3 (the next animation column).
  * group 4, golfer_stories `season_end`: P4's selected-golfer sprite read "golfer -2" (f_w_4c2e00[2] = -2), which lies in
    g_sprites5791f8; the reads go through the backend's SpriteRef fields (gfx_sprite_ref_read). Control pixel 3 (the requested
    height one larger: the sprite one row lower).
  * group 3, cov_k3_camera2 `y_inj4`: the checkpoint's POLL rode on frame 153's head clock call, so the snapshot is taken at
    that frame's first cursor read (gfx.poll_position, U-1951). Control SG_GFX_CHECKPOINT_AT=poll16 (S3.16's rule: the next
    frame's present, the spray one animation column later).
"""
from __future__ import annotations

import os
import pathlib

import numpy as np
import pytest
from PIL import Image

from port.test import framegate as FG
from port.test import gfx as GX
from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

# (scenario, checkpoint, boxes (x0, y0, x1, y1) inclusive, from log/roadmap/S3/22_rows.tsv)
ZOOM = ("cov_b_inject", "zoom_box", [(142, 89, 145, 92)])
HIRE = ("cov_r_s2_screens", "hire", [(558, 305, 586, 335), (556, 238, 580, 265), (559, 381, 582, 403), (559, 167, 579, 193)])
STORY = ("golfer_stories", "season_end", [(60, 70, 102, 90)])
SPRAY = ("cov_k3_camera2", "y_inj4", [(65, 413, 97, 451)])


@pytest.fixture(scope="module")
def glib():
    lib = L.load()
    if not lib.has("gfx.attach") or not lib.has("gditext.attach"):
        pytest.skip("the library has no gfx module / host text provider")
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    return lib


def _boxes(lib, case, tmp, negctl=None, env=None):
    """beyond-one-level differing px per box of the port's frame against the reference (None: no frame)"""
    scen, chk, boxes = case
    if not (GX.REFS / scen / "det_b" / f"{chk}.png").exists() or not SG.INSTALL.is_dir():
        pytest.skip("no pinned reference / install")
    old = {k: os.environ.get(k) for k in (env or {})}
    os.environ.update(env or {})
    try:
        FG.run_scenario(scen, [chk], lib, negctl=negctl, images=tmp, probe=False)
    finally:
        for k, v in old.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
    p = tmp / f"{scen}.{chk}.port.png"
    if not p.exists():
        return None
    port = np.array(Image.open(p).convert("RGB")).astype(int) >> 3
    ref = GX.reference(scen, chk).astype(int) >> 3
    out = []
    for x0, y0, x1, y1 in boxes:
        d = np.abs(port[y0:y1 + 1, x0:x1 + 1] - ref[y0:y1 + 1, x0:x1 + 1]).max(axis=2)
        out.append(int((d > 1).sum()))
    return out


def test_zoom_marker_equals_the_original(glib, tmp_path):
    assert _boxes(glib, ZOOM, tmp_path) == [0]


def test_zoom_marker_negctl_red(glib, tmp_path):
    got = _boxes(glib, ZOOM, tmp_path, negctl=("gfx.family.pixel", 2))
    assert got is not None and got[0] >= 10, got


def test_hire_employees_equal_the_original(glib, tmp_path):
    assert _boxes(glib, HIRE, tmp_path) == [0, 0, 0, 0]


def test_hire_employees_negctl_red(glib, tmp_path):
    got = _boxes(glib, HIRE, tmp_path, negctl=("showHireEmployeeDialog", 3))
    assert got is not None and sum(got) > 100, got


def test_selected_golfer_sprite_equals_the_original(glib, tmp_path):
    assert _boxes(glib, STORY, tmp_path) == [0]


def test_selected_golfer_sprite_negctl_red(glib, tmp_path):
    got = _boxes(glib, STORY, tmp_path, negctl=("gfx.family.pixel", 3))
    assert got is not None and got[0] > 20, got


def test_frame_head_poll_snapshot_equals_the_original(glib, tmp_path):
    assert _boxes(glib, SPRAY, tmp_path) == [0]


def test_frame_head_poll_old_rule_red(glib, tmp_path):
    got = _boxes(glib, SPRAY, tmp_path, env={"SG_GFX_CHECKPOINT_AT": "poll16"})
    assert got is not None and got[0] > 100, got


@pytest.mark.static
def test_poll_position_rules():
    heads = {100, 200}
    old = os.environ.pop("SG_GFX_CHECKPOINT_AT", None)
    try:
        assert GX.poll_position(200, 200, heads) == 199     # a POLL on the frame-head call: the head call
        assert GX.poll_position(250, 240, heads) == 240     # S3.16: a POLL before the tgt call
        assert GX.poll_position(150, 150, heads) == 150     # a POLL inside the tgt call's predecessor, no frame head there
        assert GX.poll_position(150, None, heads) == 150
        os.environ["SG_GFX_CHECKPOINT_AT"] = "poll16"
        assert GX.poll_position(200, 200, heads) == 200
        os.environ["SG_GFX_CHECKPOINT_AT"] = "tgt"
        assert GX.poll_position(250, 240, heads) == 250
    finally:
        os.environ.pop("SG_GFX_CHECKPOINT_AT", None)
        if old is not None:
            os.environ["SG_GFX_CHECKPOINT_AT"] = old
