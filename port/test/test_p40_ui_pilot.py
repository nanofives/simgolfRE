"""P40 (log/roadmap/P40/P40_notes.md): the procedural Hole Stats screen (--ui-pilot; portsim/sim/gfx_ui.h, gfx_ui.cpp, holestats.cpp).

  * OFF IS UNCHANGED: with the graphics backend attached and the pilot off, showHoleStatsScreen hands the backend exactly the calls
    it made before P40 (no p40.* call); with the pilot on, the same cases give the same sequence with each art call replaced by
    its procedural call at the same place (p40.hs_head / hs_row / hs_foot / hs_button for the HoleSTAT cells and the OkStates
    button, same y, same hover). Control: holestats mode 4 (a row one pixel low) reads RED.
  * TRANSPORT: every scene survives ui_encode / ui_decode.
  * RESOLUTION: a scene rendered at 3x and box-filtered down equals the 1x render (the same shapes at any scale). Control: gfx
    family ui mode 1 (the pixel centre half a pixel off) reads RED.
  * FIDELITY: the 1x render of the game theme against the HoleSTAT art of the user's install (interface\\infoscreens\\HoleSTAT.pcx,
    the head cell's opaque pixels): the share of pixels within 24 levels per channel stays above the pinned floor. Control: the
    alternate theme (slate) reads far below it.
"""
from __future__ import annotations

import ctypes as C
import pathlib
import random

import numpy as np
import pytest

from port.test import lib as L
from port.test.test_commands import DATA_VA, INPUT_VA, PEER_LOG_VA, fnv1a
from port.test.test_holestats import (ARRIVE_VA, CLOCK_VA, CMD_VA, PEER_LOG_CAP, SITE_OFF, cases)

ROOT = pathlib.Path(__file__).resolve().parents[2]
pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
LOG_VA = INPUT_VA + 0x40000
LOG_CAP = 0x10000
OUT_VA = 0x30000000
OUT_CAP = 1 << 22

N = {fnv1a(n): n for n in ("Surface_stretchBlit", "Image_draw", "p40.hs_head", "p40.hs_row", "p40.hs_foot", "p40.hs_button")}


def _calls(words):
    out, i = [], 0
    while i + 1 < len(words):
        h, n = words[i], words[i + 1]
        out.append((N.get(h, h), tuple(words[i + 2:i + 2 + n])))
        i += 2 + n
    return out


def run_logged(lib, case, pilot):
    img, hole, inputs, script, arrive, cur = case
    with lib.state() as st:
        st.set_data_base(DATA_VA)
        st.load_image(img)
        st.call("gfx.attach", [])
        st.call("holestats.uiPilot", [pilot])
        words = [w for c in inputs for w in c.args()] or [0]
        cbuf = (C.c_uint32 * len(words))(*words)
        st.map(CMD_VA, cbuf, readonly=True)
        abuf = (C.c_uint32 * max(1, len(arrive)))(*(arrive or [0]))
        st.map(ARRIVE_VA, abuf, readonly=True)
        pairs = [w for name, vals in script.items() for v in vals for w in (SITE_OFF[name] & 0xFFFFFFFF, v)]
        kbuf = (C.c_uint32 * len(pairs))(*pairs)
        st.map(CLOCK_VA, kbuf, readonly=True)
        st.call("holestats.setScript", [CMD_VA, len(inputs), ARRIVE_VA, CLOCK_VA, len(pairs) // 2,
                                        cur[0] & 0xFFFFFFFF, cur[1] & 0xFFFFFFFF])
        logbuf = (C.c_uint32 * (1 + 8 * PEER_LOG_CAP))()
        st.map(PEER_LOG_VA, logbuf)
        st.call("commands.recordPeer", [fnv1a("buildGolferComment"), PEER_LOG_VA, PEER_LOG_CAP])
        dbuf = (C.c_uint32 * LOG_CAP)()
        st.map(LOG_VA, dbuf)
        st.call("holestats.drawLogBegin", [])
        try:
            st.call("showHoleStatsScreen", [hole])
            code = 0
        except L.SgError as e:
            code = e.code
        n = st.call("holestats.drawLogTake", [LOG_VA, LOG_CAP]).eax
        st.call("holestats.clearScript", [])
        st.call("commands.recordClear", [])
        st.call("gfx.detach", [])
        assert n <= LOG_CAP, n
        return code, st.image(), _calls(list(dbuf[:n]))


def mapped(off_calls, on_calls):
    """'' when on_calls is off_calls with each art call replaced by its procedural call at the same place, else the first
    difference"""
    if len(off_calls) != len(on_calls):
        return f"lengths {len(off_calls)} / {len(on_calls)}"
    for k, ((a, x), (b, y)) in enumerate(zip(off_calls, on_calls)):
        if a == b and x == y:
            if isinstance(a, str) and a.startswith("p40."):
                return f"call {k}: {a} with the pilot off"
            continue
        ok = False
        if a == "Surface_stretchBlit" and b in ("p40.hs_head", "p40.hs_row", "p40.hs_foot"):
            ok = y[0] == x[4] and (b == "p40.hs_head" or len(y) == 2)
        elif a == "Image_draw" and b == "p40.hs_button":
            ok = y[0] == x[2] and y[1] == x[3] and y[2] in (0, 1)
        if not ok:
            return f"call {k}: {a}{x[:6]} vs {b}{y[:6]}"
    return ""


def _cases(lib, n=60):
    return cases(lib, n, 40)


@need_exe
def test_option_off_makes_the_original_calls(portlib):
    bad, seen, rows = [], 0, 0
    for k, case in enumerate(_cases(portlib)):
        c0, i0, off = run_logged(portlib, case, 0)
        c1, i1, on = run_logged(portlib, case, 1)
        assert c0 == c1 and i0 == i1, f"case {k}: the pilot changed the run (codes {c0} / {c1} or the image)"
        assert not any(isinstance(n, str) and n.startswith("p40.") for n, _ in off), f"case {k}: a p40 call with the pilot off"
        seen += sum(1 for n, _ in on if isinstance(n, str) and n.startswith("p40."))
        rows += sum(1 for n, _ in on if n == "p40.hs_row")
        d = mapped(off, on)
        if d:
            bad.append((k, d))
    assert not bad, f"{len(bad)} cases differ: {bad[:3]}"
    print(f"SEEN p40 calls {seen}, rows {rows}")
    assert seen > 0 and rows > 0, "no case reached the procedural calls (the backend did not attach?)"


@need_exe
def test_negctl_row_offset_reads_red(portlib):
    portlib.negctl("showHoleStatsScreen", 4)
    try:
        red = 0
        for case in _cases(portlib, 30):
            _, _, off = run_logged(portlib, case, 0)
            _, _, on = run_logged(portlib, case, 1)
            red += bool(mapped(off, on))
    finally:
        portlib.negctl_reset()
    assert red > 0, "holestats mode 4 is blind"


def render(lib, which, theme, scale, x=574, y=300, hov=0):
    with lib.state() as st:
        buf = (C.c_uint32 * OUT_CAP)()
        st.map(OUT_VA, buf)
        n = st.call("holestats.uiRender", [which, theme, x, y, hov, scale, OUT_VA, OUT_CAP]).eax
        assert n > 4, (which, theme, scale)
        w, h, x0, y0 = buf[0], buf[1], buf[2], buf[3]
        px = np.frombuffer(bytes(buf)[16:16 + 4 * w * h], dtype=np.uint8).reshape(h, w, 4).astype(np.float64)
        return px, x0, y0


@pytest.mark.parametrize("which", [0, 1, 2, 3])
@pytest.mark.parametrize("theme", [1, 2])
def test_codec_round_trip(portlib, which, theme):
    with portlib.state() as st:
        assert st.call("holestats.uiCodec", [which, theme]).eax == 1


def scale_gap(lib, which):
    one, _, _ = render(lib, which, 1, 1)
    three, _, _ = render(lib, which, 1, 3)
    h, w = one.shape[:2]
    down = three[:3 * h, :3 * w].reshape(h, 3, w, 3, 4).mean(axis=(1, 3))
    d = np.abs(down - one).max(axis=2)
    return float(d.mean()), float(np.percentile(d, 99.5))


SCALE_MEAN = {0: 0.6, 2: 0.6, 3: 1.5}   # the button (which 3) is a 44 x 42 picture made mostly of edges


@pytest.mark.parametrize("which", [0, 2, 3])
def test_render_is_resolution_independent(portlib, which):
    mean, p995 = scale_gap(portlib, which)
    print(f"SCALE which {which}: mean {mean:.3f} p99.5 {p995:.2f}")
    assert mean < SCALE_MEAN[which] and p995 < 40, (mean, p995)


def test_negctl_half_pixel_reads_red(portlib):
    portlib.negctl("gfx.family.ui", 1)
    try:
        mean, p995 = scale_gap(portlib, 0)
    finally:
        portlib.negctl_reset()
    print(f"SCALE control: mean {mean:.3f} p99.5 {p995:.2f}")
    assert not (mean < 0.6 and p995 < 40), "gfx.family.ui mode 1 is blind"


def art_head():
    from PIL import Image
    p = ROOT / "original" / "Interface" / "infoscreens"
    im, al = Image.open(p / "HoleSTAT.pcx"), Image.open(p / "HoleSTAT_alpha.pcx")
    rgb = np.asarray(im.convert("RGB"), dtype=np.float64)[:220]
    idx = np.asarray(im)[:220]
    a = np.asarray(al.convert("L"))[:220]
    return rgb, (idx != 0xFF) & (a == 255)


FIDELITY_FLOOR = 0.93   # measured 0.946 (2026-11-21, log/roadmap/P40/P40_notes.md)


def fidelity(lib, theme):
    rgb, opaque = art_head()
    px, x0, y0 = render(lib, 0, theme, 1)
    h, w = px.shape[:2]
    a = px[..., 3:4] / 255.0
    mine = np.where(a > 0, px[..., :3] / np.maximum(a, 1e-6), 0)
    region = np.zeros((220, 800), bool)
    region[y0:y0 + h, x0:x0 + w] = True
    m = opaque & region
    m[y0:y0 + h, x0:x0 + w] &= px[..., 3] >= 254
    full = np.zeros((220, 800, 3))
    full[y0:y0 + h, x0:x0 + w] = mine
    # a pixel matches when the art within one pixel of it is within 24 levels per channel (outlines redrawn by shapes land up to
    # a pixel off the painted ones)
    best = np.full((220, 800), 1e9)
    pad = np.pad(rgb, ((1, 1), (1, 1), (0, 0)), mode="edge")
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            best = np.minimum(best, np.abs(full - pad[1 + dy:221 + dy, 1 + dx:801 + dx]).max(axis=2))
    d = best[m]
    return float((d <= 24).mean()), int(m.sum())


@need_exe
def test_game_theme_matches_the_art(portlib):
    share, n = fidelity(portlib, 1)
    alt, _ = fidelity(portlib, 2)
    print(f"FIDELITY game {share:.3f} slate {alt:.3f} over {n} pixels")
    assert n > 50000
    assert share >= FIDELITY_FLOOR, share
    assert alt < 0.5, f"the slate control matches the art ({alt})"


# ------------------------------------------------------------------------------------------------ P40B: themes as data
def _theme_call(lib, entry, text):
    with lib.state() as st:
        buf = C.create_string_buffer(text.encode() + b"\0")
        st.map(OUT_VA, buf, readonly=True)
        return st.call(entry, [OUT_VA]).eax


def test_builtin_themes_load(portlib):
    """ui_themes.inc parses: game is theme 1, slate theme 2 (the indexes --ui-theme and the tests use)"""
    assert _theme_call(portlib, "holestats.uiThemeIndex", "game") == 1
    assert _theme_call(portlib, "holestats.uiThemeIndex", "slate") == 2
    assert _theme_call(portlib, "holestats.uiThemeIndex", "nosuch") == 0


def test_theme_file_adds_and_refuses(portlib):
    """a section adds a theme (from game, or its base); a malformed line refuses the whole text"""
    n = _theme_call(portlib, "holestats.uiThemeLoad", "[p40b_test]\nbase = slate\nframe = 10 20 30\n")
    assert n >= 3 and _theme_call(portlib, "holestats.uiThemeIndex", "p40b_test") == n
    assert _theme_call(portlib, "holestats.uiThemeLoad", "[bad]\nframe = 1 2\n") == 0
    assert _theme_call(portlib, "holestats.uiThemeLoad", "[bad2]\nnot_a_key = 1\n") == 0
    assert _theme_call(portlib, "holestats.uiThemeIndex", "bad") == 0


# ------------------------------------------------------------------------------------------------ P40B: the report art cells
# (gfx_ui.cpp ui_cell_scene over the cells of port/test/p40_cells.py). FLOOR: the share of the art's opaque pixels the game
# theme's 1x render matches (24 levels, 1 px of offset; the art's own colours, art_colours = 1), pinned 0.03 under the values
# measured on 2026-11-21 after batch 3 (log/roadmap/P40/cells_fidelity.txt). Control: the slate theme on the same cell reads at least 0.1 lower.
from port.test import p40_cells as PC  # noqa: E402

CELL_FLOOR = {"finance": 0.866, "course_head": 0.983, "course_row": 0.800, "course_subtotal": 0.881, "course_foot": 0.973, "sga": 0.907, "comments_head": 0.833, "comments_row": 0.937, "comments_foot": 0.942, "roster": 0.857, "lowscore_head": 0.898, "lowscore_row": 0.937, "lowscore_foot": 0.943, "histograph": 0.951, "hire": 0.942, "tour_head": 0.932, "tour_b1": 0.839, "tour_b2": 0.897, "tour_b3": 0.935, "tour_end1": 1.000, "tour_end2": 0.926, "eoy_top": 0.946, "eoy_bottom": 0.868, "eoy_line": 0.924, "shortcuts": 0.879, "route_course": 0.772, "route_bottom": 0.947, "buy_land": 0.843}


@need_exe
@pytest.mark.parametrize("cell", PC.CELLS, ids=[c[0] for c in PC.CELLS])
def test_cell_scene_matches_the_art(portlib, cell):
    game, n = PC.fidelity(portlib, cell, 1)
    slate, _ = PC.fidelity(portlib, cell, 2)
    print(f"CELL {cell[0]}: game {game:.3f} slate {slate:.3f} over {n} px")
    assert n > 1000
    assert game >= CELL_FLOOR[cell[0]] - 0.03, game
    if not cell[0].endswith(("_row", "_line")):   # a row band or line strip is almost all cream box, which slate keeps light: blind
        assert slate <= game - 0.1, f"the slate control matches the art as well ({slate:.3f})"


@need_exe
@pytest.mark.parametrize("name", ["finance", "course_foot", "lowscore_head"])
def test_cell_scene_is_resolution_independent(portlib, name):
    cell = next(c for c in PC.CELLS if c[0] == name)
    one, *_ = PC.cell_render(portlib, cell, 1, 1)
    three, *_ = PC.cell_render(portlib, cell, 1, 3)
    h, w = one.shape[:2]
    down = three[:3 * h, :3 * w].reshape(h, 3, w, 3, 4).mean(axis=(1, 3))
    d = np.abs(down - one).max(axis=2)
    print(f"CELLSCALE {name}: mean {d.mean():.3f} p99.5 {np.percentile(d, 99.5):.2f}")
    assert d.mean() < 2.5 and np.percentile(d, 99.5) < 60   # one-pixel rims (thin fields) differ most between 1x and 3x


# ------------------------------------------------------------------------------------------------ P40C: the HUD panels (border-only)
# The panels' outline as the vector shape, the painted inside kept as art (gfx_ui.h ui_cell_scene border_only). FLOOR: the share of
# the drawn band's pixels matching the art (as above), pinned 0.03 under the 2026-11-21 values.
HUD_FLOOR = {"hud_terrain": 0.917, "hud_building": 0.864, "hud_amenities": 0.992, "hud_member": 0.864, "hud_action": 0.998, "hud_match": 0.705, "hud_employee": 0.866, "hud_elevation": 0.998, "hud_golfer": 0.833, "hud_story": 0.895}


@need_exe
@pytest.mark.parametrize("cell", PC.HUD_CELLS, ids=[c[0] for c in PC.HUD_CELLS])
def test_hud_panel_border_matches_the_art(portlib, cell):
    game, n = PC.fidelity(portlib, cell, 1)
    _, keep, col, msk, _ = PC.cell_render(portlib, cell, 1, 1)
    inside = float((keep[(col != 0xFF) & (msk == 0)] == 1).mean())
    print(f"HUD {cell[0]}: band {game:.3f} over {n} px, kept inside {inside:.2f}")
    assert n > 500
    assert game >= HUD_FLOOR[cell[0]] - 0.03, game
    assert inside > 0.5, "border-only kept less than half of the panel as art"
