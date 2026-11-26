"""P41 port pilot (log/roadmap/P41/PORT_PILOT_BRIEF.md, log/roadmap/P41/pilot/notes.md): the title menu's buttons drawn from the
approved uivec scenes (portsim/sim/gfx_uivec.h, port/src/sim/gfx_uivec.cpp, port/src/sim/uivec_title.inc).

  * PARITY: every exported title scene (6 buttons x idle / hover) rendered by the DLL (uivec.render) equals re/tools/uivec/sdf.py's
    sdf.render of the same scene (title.scene of the approved geometry, NOT the exported one: the exporter's expansions are judged
    too), at 1x and at 2.7x, over a black and over a white background (so colour and coverage are both judged): over the pixels
    either render touches, mean absolute difference <= 1.0 level and no channel over 12. One line per scene `[N/M] name STATUS`.
    Controls: uivec 1 (the pixel centre half a pixel off) and 2 (the bevel's normal upside down) read RED.
  * EXPORT: port/src/sim/uivec_title.inc is what re/tools/uivec/port_export.py writes now (the approved scenes, not a stale copy).
  * OFF IS THE ORIGINAL: with the UI theme off, the title menu's Image_draw of every button cut (uivec.drawTitle: the drop the menu
    makes) draws exactly what the backend draws with no theme ever set (the original body: the art), and the cut's pixels change;
    with the theme on, the same drop changes nothing outside the scene's bounds and gives the scene's 1x render over the root as it
    was (RGB555, within one 5-bit level).

P41W (log/roadmap/P41/PORT_WIDE_BRIEF.md, log/roadmap/P41/portwide/notes.md): the wide front-end backdrops (portsim/sim/gfx_uvwide.h,
port/src/sim/gfx_uvwide.cpp, port/src/sim/uivec_wide_<screen>.inc). Tests in port/test/p41_wide_cases.py, imported below.

  * WIDE PARITY: each wide screen's static backdrop rendered by the DLL (uvwide.render) equals wide.py's render of the approved wide
    scene (the overlays the game draws itself left out; pictures lifted from the game's sheets with the scene's mattes) at 4:3
    (800 x 600) and 16:9 (1067 x 600), and on crops at 2.7x (the publisher marks: the picture upsampled) and at 3.6x (4K: the ground
    upsampled from kUwGroundScaleMax): mean absolute difference <= 1.0 level and no channel over 12. Each picture run
    (uvwide.picRender) equals composite_picture of the lifted picture over black and white at 1x and 2.7x. Controls uvwide 1 (the
    blobs' softness doubled), 2 (the upsampled ground misregistered) and 3 (a ground_diff matte without its feather) read RED.
  * WIDE EXPORT: port/src/sim/uivec_wide_<screen>.inc is what port_export.py --wide writes now.
  * WIDE OFF IS THE ORIGINAL: with the theme off the title menu's full-screen copy of TitleBASE (uvwide.drawTitleBg) draws exactly the
    original body's art and marks no backdrop; with the theme on it draws the backdrop's 1x frame (uvwide.frame) and marks the title;
    negative control uvwide 4 (the hook off) draws the art. The logo cut (13) is art with the theme off and with no backdrop shown.
"""
from __future__ import annotations

import ctypes as C
import pathlib
import subprocess
import sys

import numpy as np
import pytest

from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
UIVEC = ROOT / "re" / "tools" / "uivec"
sys.path.insert(0, str(UIVEC))
import port_export  # noqa: E402
import sdf  # noqa: E402

pytestmark = pytest.mark.port
OUT_VA = 0x30000000
OUT_CAP = 1 << 23   # words
NAME_VA = 0x2f000000
INSTALL = ROOT / "original"
MAD_MAX, PX_MAX = 1.0, 12.0


@pytest.fixture(scope="module")
def uvlib(portlib):
    if not portlib.has("uivec.render"):
        pytest.skip("the library has no gfx_uivec module")
    return portlib


@pytest.fixture(scope="module")
def scenes():
    return port_export.scenes()


def dll_render(lib, k, scale, dx=0, dy=0):
    with lib.state() as st:
        buf = (C.c_uint32 * OUT_CAP)()
        st.map(OUT_VA, buf)
        n = st.call("uivec.render", [k, dx & 0xFFFFFFFF, dy & 0xFFFFFFFF, int(round(scale * 1000)), OUT_VA, OUT_CAP]).eax
        assert n > 4, (k, scale)
        w, h, x0, y0 = (C.c_int32 * 4).from_buffer(buf)[:]
        prem = np.frombuffer(bytes(buf)[16:16 + 16 * w * h], dtype="<f4").reshape(h, w, 4).astype(np.float64)
        return prem, x0, y0


def compare(lib, k, items, scale):
    """(mad, worst) over the black and the white background, the pixels either render touches"""
    prem, x0, y0 = dll_render(lib, k, scale)
    h, w = prem.shape[:2]
    mads, worst = [], 0.0
    for bg in (0.0, 255.0):
        ref = np.full((h, w, 3), bg)
        sdf.render(items, ref, scale, origin=(x0, y0))
        got = prem[..., :3] + (1.0 - prem[..., 3:4]) * bg
        touched = (prem[..., 3] > 0) | (np.abs(ref - bg).max(axis=2) > 0)
        d = np.abs(got - ref)[touched]
        mads.append(float(d.mean()) if d.size else 0.0)
        worst = max(worst, float(d.max()) if d.size else 0.0)
    return max(mads), worst


def test_scene_table_matches_the_exporter(uvlib, scenes):
    with uvlib.state() as st:
        assert st.call("uivec.count", []).eax == len(scenes)
        vals = [st.call("uivec.scene", [k]).eax for k in range(len(scenes))]
    for k, (name, button, hover, _) in enumerate(scenes):
        v = vals[k]
        assert (v & 0xFF, (v >> 8) & 0xFF, v >> 16) == (button, hover, port_export.SLOTS[name] + hover), (k, name)


def test_include_is_the_exporters_output():
    r = subprocess.run([sys.executable, str(UIVEC / "port_export.py"), "--check"], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr


@pytest.mark.parametrize("scale", [1.0, 2.7])
def test_render_equals_sdf(uvlib, scenes, scale):
    bad = []
    for k, (name, button, hover, items) in enumerate(scenes):
        mad, worst = compare(uvlib, k, items, scale)
        ok = mad <= MAD_MAX and worst <= PX_MAX
        print(f"[{k + 1}/{len(scenes)}] {name}.{'hover' if hover else 'idle'} x{scale} {'PASS' if ok else 'FAIL'} "
              f"mad {mad:.2e} max {worst:.2e}", flush=True)
        if not ok:
            bad.append((name, hover, mad, worst))
    assert not bad, bad


@pytest.mark.parametrize("mode", [1, 2])
def test_negctl_reads_red(uvlib, scenes, mode):
    uvlib.negctl("uivec.render", mode)
    try:
        red = 0
        judged = 0
        for k, (name, button, hover, items) in enumerate(scenes):
            # mode 2 turns the bevel paint's normal: judged on the scenes that have a bevel (all twelve title scenes do)
            if mode == 2 and not any(isinstance(L["fill"], dict) for it in items for L in it["layers"]):
                continue
            judged += 1
            mad, worst = compare(uvlib, k, items, 1.0)
            red += mad > MAD_MAX or worst > PX_MAX
    finally:
        uvlib.negctl_reset()
    print(f"uivec mode {mode}: {red} / {judged} scenes RED")
    assert judged >= 12 and red == judged, f"uivec mode {mode} is blind on {judged - red} of {judged} scenes"


# ------------------------------------------------------------------------------------------------ off is the original
ROOT_W, ROOT_H = 800, 600


def _root(st):
    assert st.call("gfx.dump", [1]).eax == 0
    return np.frombuffer(st.files_read("$gfx/root.555"), "<u2").reshape(ROOT_H, ROOT_W).copy()


def _title_state(lib, pilot):
    st = lib.state()
    st.fill_from_exe()
    st.files_root(INSTALL)
    st.call("gfx.attach", [])
    namebuf = st.map(NAME_VA, bytearray(0x100))
    raw = b"showTitleScreen\0"
    C.memmove(C.addressof(namebuf), raw, len(raw))
    assert st.call("gfx.call", [NAME_VA] + [0] * 11).eax == 1
    if pilot is not None:
        st.call("holestats.uiPilot", [pilot])
    return st


def _over555(prem, word):
    out = np.zeros_like(word)
    for ch, sh in enumerate((10, 5, 0)):
        lv = (word >> sh) & 31
        e = ((lv << 3) | (lv >> 2)).astype(np.float64)
        v = np.clip(prem[..., ch] + (1.0 - prem[..., 3]) * e, 0, 255)
        out |= (np.minimum(31, np.floor(v + 1e-3).astype(np.int64) >> 3).astype(np.uint16) << sh)   # gfx_ui.cpp ui_over555 (c >> 3)
    return out


need_install = pytest.mark.skipif(not (INSTALL / "golf_clean.exe").exists(), reason="no install (original/)")


@need_install
def test_option_off_makes_the_original_calls(uvlib, scenes):
    if uvlib.exe_load():
        pytest.skip("no exe image")
    drawn_on = 0
    for k, (name, button, hover, items) in enumerate(scenes):
        slot = port_export.SLOTS[name] + hover
        cx, cy = port_export.title.CELLS[name][:2]
        roots = {}
        for pilot in (None, 0, 1):
            with _title_state(uvlib, pilot) as st:
                before = _root(st)
                assert st.call("uivec.drawTitle", [slot, cx, cy]).eax == 1, (name, hover, pilot)
                roots[pilot] = (before, _root(st))
        b0, art = roots[None]
        assert (roots[0][1] == art).all(), f"{name}.{hover}: the theme off does not draw the original's art"
        assert (art != b0).any(), f"{name}.{hover}: the art drew nothing (no cut loaded?)"
        b1, on = roots[1]
        prem, x0, y0 = dll_render(uvlib, k, 1.0)
        h, w = prem.shape[:2]
        outside = np.ones((ROOT_H, ROOT_W), bool)
        outside[y0:y0 + h, x0:x0 + w] = False
        assert (on[outside] == b1[outside]).all(), f"{name}.{hover}: the scene changed pixels outside its bounds"
        want = b1.copy()
        sub = want[y0:y0 + h, x0:x0 + w]
        m = prem[..., 3] > 0
        sub[m] = _over555(prem, sub)[m]
        diff = 0
        for sh in (10, 5, 0):
            diff = np.maximum(diff, np.abs(((on >> sh) & 31).astype(int) - ((want >> sh) & 31).astype(int)))
        assert diff.max() <= 1, f"{name}.{hover}: the root drawing is not the scene's 1x render (max {diff.max()} levels)"
        assert (on != art).any(), f"{name}.{hover}: the theme on drew only the art"
        drawn_on += 1
        print(f"[{k + 1}/{len(scenes)}] {name}.{'hover' if hover else 'idle'} off=art on=scene PASS", flush=True)
    assert drawn_on == len(scenes)


# P41W: the wide front-end backdrops (port/test/p41_wide_cases.py)
from port.test import p41_wide_cases as _wide  # noqa: E402

globals().update(_wide.make(port_export, sdf, UIVEC, INSTALL, _title_state, _root))

# P41G: the in-game element scenes (port/test/p41_game_cases.py)
from port.test import p41_game_cases as _game  # noqa: E402

globals().update(_game.make(sdf, UIVEC, INSTALL, _root, _over555))
