"""The framegate split verdict (roadmap S3.23, decision G18; port/test/framegate.py --split).

A differing pixel is TERRAIN-ORIGIN when it differs from the reference in the GlRaster frame and equals it in the host-GL frame
(the host GL equals the original's terrain bit for bit, S3.15); every other differing pixel is 2D. The split verdict needs 2D px = 0
in every region and the terrain-origin px within G2 (glraster.MAX_ONE_LEVEL of the view area, glraster.MAX_BIG beyond one level).

    ~/bin/py -3.12 -m pytest port/test/test_framegate_split.py -q

Unit tests run on synthetic frames. The run tests boot sandbox_basic (Windows, host OpenGL) and use the test seam
SG_FRAMEGATE_PLANT=<both|raster>:<region>:<n> (framegate.run_scenario_split) and negctl gfx.family.terrain=12 (no host pixel format).
Controls: a 2D difference planted in hud_title stays FAIL under the split; a terrain-only difference beyond G2 reads FAIL as
terrain-origin; no host-GL frame reads UNJUDGED, never PASS.
"""
from __future__ import annotations

import sys

import numpy as np
import pytest

from port.test import framegate as FG
from port.test import glraster as GR

pytestmark = pytest.mark.port
W, H = FG.W, FG.H
VIEW = (0x100, 0x100, 0x100, 0x100)      # inside the in-game `view` remainder (no HUD region reaches it)
HUD = dict(FG.INGAME_REGIONS)["hud_title"]


def _ref():
    return np.full((H, W, 3), 0x80, np.uint8)


def _bump(img, rect, n, delta):
    out = img.copy()
    x0, y0, w, _h = rect
    for i in range(n):
        out[y0 + i // w, x0 + i % w, 0] = int(out[y0 + i // w, x0 + i % w, 0]) + delta
    return out


def _split(raster, host, kind="ingame", terrain=True):
    ref = _ref()
    return FG.split_row("FAIL", FG.split_numbers(raster, host, ref, kind, terrain))


def test_identical_frames_pass():
    r = _split(_ref(), _ref())
    assert r["split_status"] == "PASS" and r["twod_px"] == 0 and r["terrain_origin_px"] == 0


def test_terrain_residue_within_g2_passes():
    # one-level px in the view under GlRaster only, 1 % of the view area: terrain-origin, inside G2
    n = int(0.01 * FG.label_map("ingame")[0].size * 0.5)
    r = _split(_bump(_ref(), (0x100, 0x100, 0x100, 0x100), min(n, 60000), 8), _ref())
    assert r["split_status"] == "PASS", r
    assert r["terrain_origin_px"] == min(n, 60000) and r["twod_px"] == 0


def test_terrain_residue_in_a_hud_region_is_terrain_origin_not_2d():
    r = _split(_bump(_ref(), HUD, 30, 8), _ref())
    assert r["twod_px"] == 0 and r["terrain_origin_px"] == 30 and r["split_status"] == "PASS"


def test_control_2d_difference_in_hud_stays_fail():
    # the same pixels differ in both frames: not terrain, so 2D in hud_title
    f = _bump(_ref(), HUD, 5, 8)
    r = _split(f, f)
    assert r["split_status"] == "FAIL" and r["twod_px"] == 5 and r["terrain_origin_px"] == 0
    assert "hud_title=5" in r["split_detail"] and "hud_title=5/" in r["split_regions"]


def test_control_2d_difference_hidden_by_terrain_residue_still_fails():
    # a 2D pixel under the host GL plus terrain residue elsewhere in the same region: only the 2D pixel counts as 2D
    host = _bump(_ref(), HUD, 1, 8)
    raster = _bump(host, (HUD[0], HUD[1] + 20, HUD[2], 8), 40, 8)
    r = _split(raster, host)
    assert r["twod_px"] == 1 and r["terrain_origin_px"] == 40 and r["split_status"] == "FAIL"


def test_control_terrain_only_difference_beyond_g2_fails_as_terrain_origin():
    over = GR.MAX_BIG + 1
    r = _split(_bump(_ref(), VIEW, over, 24), _ref())
    assert r["split_status"] == "FAIL" and r["twod_px"] == 0
    assert r["terrain_origin_beyond"] == over and "terrain-origin over G2" in r["split_detail"]
    ok = _split(_bump(_ref(), VIEW, GR.MAX_BIG, 24), _ref())          # at the bound it passes
    assert ok["split_status"] == "PASS"


def test_one_level_allowance_is_a_fraction_of_the_view_area():
    area = FG.label_map("ingame")[0]
    view_area = int((area == len(FG.INGAME_REGIONS)).sum())
    n = int(GR.MAX_ONE_LEVEL * view_area) + 1
    # n px spread over the view rows (several rows of the 800-wide view)
    img = _ref()
    ys, xs = np.nonzero(area == len(FG.INGAME_REGIONS))
    img[ys[:n], xs[:n], 0] += 8
    r = _split(img, _ref())
    assert r["split_status"] == "FAIL" and r["terrain_origin_px"] == n and r["terrain_origin_beyond"] == 0


def test_no_terrain_on_screen_makes_every_pixel_2d():
    r = _split(_bump(_ref(), VIEW, 10, 8), _ref(), kind="frame", terrain=False)
    assert r["twod_px"] == 10 and r["terrain_origin_px"] == 0 and r["split_status"] == "FAIL"


def test_a_pixel_wrong_only_with_the_host_gl_is_2d():
    r = _split(_ref(), _bump(_ref(), VIEW, 7, 8))
    assert r["twod_px"] == 7 and r["split_status"] == "FAIL"


def test_no_host_gl_frame_is_unjudged_never_pass():
    r = FG.split_row("FAIL", None, "terrainLive.hostGl: the host has no OpenGL implementation the sink can use")
    assert r["split_status"] == "UNJUDGED" and "hostGl" in r["split_detail"]
    assert FG.split_row("PASS", None)["split_status"] == "UNJUDGED"


def test_unjudged_raw_rows_keep_their_status():
    assert FG.split_row("NOT_RUN", None)["split_status"] == "NOT_RUN"
    assert FG.split_row("UNSUPPORTED", None)["split_status"] == "UNSUPPORTED"


def test_plant_changes_only_equal_pixels_inside_the_rectangle():
    ref = _ref()
    other = _bump(ref, VIEW, 3, 8)
    out = FG.plant(ref, VIEW, 10, ref, other)
    d = (out != ref).any(axis=2)
    assert d.sum() == 10 and not d[:VIEW[1]].any()
    assert not (d & (other != ref).any(axis=2)).any()


def test_tsv_columns_unchanged_without_split(tmp_path):
    rows = [{"scenario": "s", "checkpoint": "c", "status": "PASS"}]
    FG.write_tsv(tmp_path / "a.tsv", rows)
    assert (tmp_path / "a.tsv").read_text().splitlines()[0].split("\t") == FG.COLUMNS
    FG.write_tsv(tmp_path / "b.tsv", [dict(rows[0], split_status="PASS")])
    assert (tmp_path / "b.tsv").read_text().splitlines()[0].split("\t") == FG.COLUMNS + FG.SPLIT_COLUMNS


# ----------------------------------------------------------------------------------------------- runs (boot sandbox_basic)
@pytest.fixture(scope="module")
def lib():
    if sys.platform != "win32":
        pytest.skip("the host-OpenGL backend is Windows' generic opengl32")
    from port.test import lib as L
    return L.load()


def _first(rows):
    return {r["checkpoint"]: r for r in rows}["sandbox_00"]


def test_run_split_keeps_the_raw_columns_and_judges_the_residue(lib):
    plain = _first(FG.run_scenario("sandbox_basic", lib=lib, probe=False, checkpoints=["sandbox_00"]))
    sp = _first(FG.run_scenario_split("sandbox_basic", lib=lib, probe=False, checkpoints=["sandbox_00"]))
    for k in FG.COLUMNS:
        if k not in ("seconds",):
            assert sp.get(k) == plain.get(k), k
    assert plain["status"] == "FAIL"                     # I14's 125 one-level px under GlRaster
    assert sp["split_status"] == "PASS" and sp["twod_px"] == 0 and sp["terrain_origin_px"] > 0


def test_run_control_2d_plant_in_hud_stays_fail(lib, monkeypatch):
    monkeypatch.setenv("SG_FRAMEGATE_PLANT", "both:hud_title:6")
    sp = _first(FG.run_scenario_split("sandbox_basic", lib=lib, probe=False, checkpoints=["sandbox_00"]))
    assert sp["split_status"] == "FAIL" and sp["twod_px"] >= 6 and "hud_title" in sp["split_detail"]


def test_run_control_terrain_only_plant_reads_fail_as_terrain_origin(lib, monkeypatch):
    monkeypatch.setenv("SG_FRAMEGATE_PLANT", f"raster:view:{GR.MAX_BIG + 40}")
    sp = _first(FG.run_scenario_split("sandbox_basic", lib=lib, probe=False, checkpoints=["sandbox_00"]))
    assert sp["split_status"] == "FAIL" and sp["twod_px"] == 0
    assert int(sp["terrain_origin_beyond"]) >= GR.MAX_BIG + 40 and "terrain-origin over G2" in sp["split_detail"]


def test_run_without_a_host_pixel_format_is_unjudged(lib):
    sp = _first(FG.run_scenario_split("sandbox_basic", lib=lib, probe=False, checkpoints=["sandbox_00"],
                                      negctl=("gfx.family.terrain", 12)))
    assert sp["split_status"] == "UNJUDGED" and "hostGl" in sp["split_detail"]
