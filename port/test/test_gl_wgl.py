"""The host-OpenGL terrain backend (roadmap S3.15, U-1810; port/include/portsim/sim/gl_wgl.h): with terrainLive.hostGl(1) the
terrain passes are drawn by the host's own OpenGL 1.1 software implementation (Windows' generic opengl32 on the original's 16-bit
555 DIB) instead of GlRaster, and the in-game checkpoint frames equal the original's back buffer in every pixel where GlRaster left
its one-level residual (U-1392: I14's census had it in the HUD top bar of 446 FAIL rows, in bottom_panel and in the view).

    ~/bin/py -3.12 -m pytest port/test/test_gl_wgl.py -q          (Windows only; SG_BACKEND=win32 runs it on the Win32 runner)

Evidence: framegate (port/test/framegate.py, the census's measure) over pinned scenarios, against the original's back buffer
(scenario_refs det_b). Rows whose difference has another cause are listed in KNOWN with the cause; every other row must PASS.
"""
from __future__ import annotations

import sys

import pytest

from port.test import framegate as FG

pytestmark = pytest.mark.port

# scenario -> checkpoints that keep pixels from another cause (they must still have 0 px in the four HUD top-bar regions unless the
# cause sits there; the region and the cause are given)
KNOWN = {
    "sandbox_basic": {},
    # y_inj4: the fountain's spray sprite at (0x17..0x61, 0x19f..0x1c3), more than one level off, inside `toolbar` and `view`
    # (a scene sprite, not the terrain: log/roadmap/S3/15_notes.md, U-1812)
    "cov_k3_camera2": {"y_inj4": {"toolbar"}},
    # r20: a splash sprite in the water at (0xfa..0x122, 0xb..0x28), more than one level off, and view pixels of a strip frame
    # (U-1813)
    "cov_k1_river": {"r20": {"hud_title", "hud_message"}},
}
TOP = ("hud_title", "hud_meters", "hud_message", "toolbar")
_RUNS: dict = {}


def _regions(row) -> dict:
    out = {}
    for p in (row.get("regions") or "").split(";"):
        if "=" in p:
            k, v = p.split("=", 1)
            out[k] = int(v.split("/")[0])
    return out


@pytest.fixture(scope="module")
def lib():
    if sys.platform != "win32":
        pytest.skip("the host-OpenGL backend is Windows' generic opengl32")
    from port.test import lib as L
    return L.load()


def _run(lib, scenario, host_gl=True, negctl=None, gl="raster"):
    key = (scenario, host_gl, negctl, gl)
    if key not in _RUNS:
        _RUNS[key] = FG.run_scenario(scenario, lib=lib, negctl=negctl, probe=False, host_gl=host_gl, gl=gl)
    return _RUNS[key]


@pytest.mark.parametrize("scenario", sorted(KNOWN))
def test_host_gl_frames_equal_the_original(lib, scenario):
    rows = _run(lib, scenario)
    assert rows, scenario
    bad = []
    for r in rows:
        known = KNOWN[scenario].get(r["checkpoint"])
        if known is None:
            if r["status"] != "PASS":
                bad.append((r["checkpoint"], r["status"], r.get("detail")))
            continue
        reg = _regions(r)
        bad += [(r["checkpoint"], n, reg[n]) for n in TOP if reg.get(n) and n not in known]
    assert not bad, bad


def test_raster_backend_is_unchanged(lib):
    # GlRaster (--gl raster) keeps its residual: sandbox_00's I14 census numbers (125 px, all one level)
    r = {x["checkpoint"]: x for x in _run(lib, "sandbox_basic", host_gl=False, gl="raster")}["sandbox_00"]
    assert r["status"] == "FAIL" and int(r["total_px"]) == 125 and int(r["beyond_px"]) == 0 and r["renderer"] == "raster", r


def test_default_is_glraster_and_auto_is_the_checked_host_gl(lib):
    # roadmap S3.20 (decision G13, final): framegate's default is GlRaster; the opt-in `auto` is the host's OpenGL once its
    # self-check passes (the same rows as S3.15's --host-gl run), and `host` the host's OpenGL without the check (the same rows)
    default = _run(lib, "sandbox_basic", host_gl=False)
    auto = _run(lib, "sandbox_basic", host_gl=False, gl="auto")
    host = _run(lib, "sandbox_basic", host_gl=False, gl="host")
    forced = _run(lib, "sandbox_basic")
    key = lambda rows: [(r["checkpoint"], r["status"], r.get("regions")) for r in rows]
    assert {r["renderer"] for r in default} == {"raster"} and key(default) != key(forced)
    assert key(auto) == key(forced) == key(host)
    assert {r["renderer"] for r in auto} == {"wgl"} and all("self-check passed" in r["renderer_reason"] for r in auto)
    assert {r["renderer"] for r in host} == {"wgl"} and all("no self-check" in r["renderer_reason"] for r in host)


def test_host_gl_falls_back_without_a_pixel_format(lib):
    # control 12: the sink finds no pixel format (as a host without a generic 16-bit one): terrainLive.hostGl returns 0, framegate
    # refuses a run that requires it, and a run that only asks for it ("try") draws with GlRaster, equal to the default run
    with pytest.raises(RuntimeError, match="hostGl"):
        _run(lib, "sandbox_basic", host_gl=True, negctl=("gfx.family.terrain", 12))
    tried = _run(lib, "sandbox_basic", host_gl="try", negctl=("gfx.family.terrain", 12))
    default = _run(lib, "sandbox_basic", host_gl=False, gl="raster")
    key = lambda rows: [(r["checkpoint"], r["status"], r.get("regions")) for r in rows]
    assert key(tried) == key(default)


def test_host_gl_frames_are_the_same_on_every_run(lib):
    # the same boot twice with the host GL: every checkpoint image identical (framegate's images through region_numbers)
    imgs = []
    orig = FG.region_numbers
    for _ in range(2):
        got = []
        FG.region_numbers = lambda p, r, k, got=got: (got.append(p.tobytes()), orig(p, r, k))[1]
        try:
            FG.run_scenario("cov_k3_camera2", lib=lib, probe=False, host_gl=True)
        finally:
            FG.region_numbers = orig
        imgs.append(got)
    assert imgs[0] and imgs[0] == imgs[1]


# negative controls of the backend (gfx_terrain.cpp, family `terrain`): 8 a DIB with RGB565 masks, 10 the root surface not loaded
# into the DIB before a pass (seen where the camera moves: renderTerrainStrip's shifted copy). Blind and recorded in the notes:
# 9 rows top-down (the implementation writes the same memory rows) and 11 texture names unmapped (its names equal the port's).
@pytest.mark.parametrize("mode,scenario", [(8, "sandbox_basic"), (10, "cov_k3_camera2")])
def test_host_gl_negctl_reads_red(lib, mode, scenario):
    rows = _run(lib, scenario, negctl=("gfx.family.terrain", mode))
    good = {r["checkpoint"] for r in _run(lib, scenario) if r["status"] == "PASS"}
    red = [r["checkpoint"] for r in rows if r["checkpoint"] in good and r["status"] != "PASS"]
    assert red, f"control {mode} reads GREEN on {scenario}"
