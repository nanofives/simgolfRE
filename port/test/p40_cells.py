"""P40B: the report art cells (gfx_routing.cpp's cut tables) for the cell engine's tests (test_p40_ui_pilot.py) and previews.

CELLS: (screen, sheet, alpha sheet, cut x, y, w, h). The sheets are read from the user's install (interface\\infoscreens); nothing of
them is stored. cell_render runs holestats.uiCell (gfx_ui.h ui_cell_scene) on a cell and returns the render and the kept pixels.
    ~/bin/py -3.12 port/test/p40_cells.py <out dir>     # previews: art | game theme 1x | slate 1x per cell, and a 3x crop
"""
from __future__ import annotations

import ctypes as C
import os
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
INFO = ROOT / "original" / "Interface" / "infoscreens"
OUT_VA = 0x30000000
IN_VA = 0x34000000
OUT_CAP = 1 << 23

# gfx_routing.cpp kHead / kTail / kTail2 / kTail4 and reports.cpp / routing.cpp (the cuts drawn with Surface_stretchBlit + mask)
CELLS = [
    ("finance", "FINANCEreport", "FINANCEreport_alpha", 8, 7, 0x310, 0x121),
    ("course_head", "coursereport", "coursereport_alpha", 0, 0, 0x320, 0x68),
    ("course_row", "coursereport", "coursereport_alpha", 0, 0x81, 0x320, 0x11),
    ("course_subtotal", "coursereport", "coursereport_alpha", 0, 0x1A4, 0x320, 0x1D),
    ("course_foot", "coursereport", "coursereport_alpha", 0, 0x1D4, 0x320, 0x49),
    ("sga", "SGA", "SGAreport_alpha", 0x23, 0x1A, 0x2DA, 0x1A3),
    ("comments_head", "PlayComt", "PlayComt_alpha", 0x94, 0x2D, 0x1F9, 0x66),
    ("comments_row", "PlayComt", "PlayComt_alpha", 0x94, 0xE0, 0x1F9, 0x11),
    ("comments_foot", "PlayComt", "PlayComt_alpha", 0x94, 0x141, 0x1F9, 0x3D),
    ("roster", "memberRoster", "memberRoster_alpha", 0, 0, 0x320, 0x258),
    ("lowscore_head", "lowscore", "lowscore_alpha", 0xC3, 0x2D, 0x19B, 0x4F),
    ("lowscore_row", "lowscore", "lowscore_alpha", 0xC3, 0xE0, 0x19B, 0x11),
    ("lowscore_foot", "lowscore", "lowscore_alpha", 0xC3, 0x141, 0x19B, 0x3D),
    ("histograph", "histograph", "histograph_alpha", 0, 0, 0x320, 0x258),
    ("hire", "hire", "hire_alpha", 0, 0, 0x320, 0x258),
    ("tour_head", "tournament result", "tournament result_alpha", 0, 0, 0x320, 0x6A),
    ("tour_b1", "tournament result", "tournament result_alpha", 0, 0xB4, 0x320, 0x1A),
    ("tour_b2", "tournament result", "tournament result_alpha", 0, 0x7D, 0x320, 0x16),
    ("tour_b3", "tournament result", "tournament result_alpha", 0, 0xE0, 0x320, 0x12),
    ("tour_end1", "tournament result", "tournament result_alpha", 0, 0x165, 0x320, 0x33),
    ("tour_end2", "tournament result", "tournament result_alpha", 0, 0x112, 0x320, 0x33),
    # P40B batch 3: End of Year (reports.cpp), Shortcuts (routing.cpp), the route screens and buy-land (Image_draw, no mask sheet)
    ("eoy_top", "ENDoYEAR", "ENDoYEAR_alpha", 0xBB, 0x28, 0x1AD, 0xB0),
    ("eoy_bottom", "ENDoYEAR", "ENDoYEAR_alpha", 0xBB, 0x176, 0x1AD, 0x37),
    ("eoy_line", "ENDoYEAR", "ENDoYEAR_alpha", 0xBB, 0x123, 0x1AD, 0xF),
    ("shortcuts", "shortcuts", "shortcuts_alpha", 0, 0, 0x320, 0x199),
    ("route_course", "route screens_course", None, 0, 0, 0x320, 0xFD),
    ("route_bottom", "route screens_bottom", None, 0, 0xFD, 0x320, 0x15B),
    ("buy_land", "buy_land", None, 0, 0, 0x320, 0x258),
]
# P40C batch 4: the HUD panels (g_sprites541a18 over g_sprites58b390, loadInterfaceArt; the interface folder)
HUD_CELLS = [
    ("hud_terrain", "../BaseTerrainPanel", "../BaseTerrainPanel_A", 0xD8, 0x1E2, 0x248, 0x76),
    ("hud_building", "../BuildingPanel", "../BuildingPanel_A", 0xD8, 0x1E2, 0x248, 0x76),
    ("hud_amenities", "../AmenitiesPanel", "../AmenitiesPanel_A", 0xD6, 0x1E2, 0x24A, 0x76),
    ("hud_member", "../MemberPanel", "../MemberPanel_A", 0xD7, 0x1DA, 0x249, 0x7E),
    ("hud_action", "../JoeCoolPanel", "../JoeCoolPanel_A", 0xD6, 0x1DA, 0x24A, 0x7E),
    ("hud_match", "../JoeCoolPanel", "../JoeCoolPanel_A", 0x129, 0x12C, 0x1F7, 0x7A),
    ("hud_employee", "../EmployeePanel", "../EmployeePanel_A", 0xD7, 0x1DA, 0x249, 0x7E),
    ("hud_elevation", "../ElevationPanel", "../ElevationPanel_A", 0xD7, 0x1E2, 0x249, 0x76),
    ("hud_golfer", "../GolferStats", "../GolferStats_A", 0x40, 0x21, 0x192, 0xF4),
    ("hud_story", "../GolferStats", "../GolferStats_A", 0x40, 0x12C, 0x192, 0x8A),
]


def cell_bytes(sheet, alpha, x, y, w, h):
    from PIL import Image
    im = Image.open(INFO / f"{sheet}.pcx")
    col = np.asarray(im, dtype=np.uint8)[y:y + h, x:x + w].copy()
    if alpha is None:   # Image_draw's cells: draw16 skips the indexes 0xfe / 0xff
        msk = np.where(col >= 0xFE, 0xFF, 0).astype(np.uint8)
    else:
        msk = np.asarray(Image.open(INFO / f"{alpha}.pcx"), dtype=np.uint8)[y:y + h, x:x + w].copy()
    pal = im.getpalette()
    p555 = np.array([((pal[3 * i] >> 3) << 10) | ((pal[3 * i + 1] >> 3) << 5) | (pal[3 * i + 2] >> 3) for i in range(256)], np.uint16)
    rgb = np.asarray(im.convert("RGB"), dtype=np.float64)[y:y + h, x:x + w]
    return col, msk, p555, rgb


def cell_render(lib, cell, theme, scale):
    _, sheet, alpha, x, y, w, h = cell
    col, msk, pal, rgb = cell_bytes(sheet, alpha, x, y, w, h)
    with lib.state() as st:
        def page(b):   # mappings are whole pages
            n = (len(b) + 0xFFF) & ~0xFFF
            return (C.c_uint8 * n).from_buffer_copy(b.ljust(n, bytes(1)))
        cb, mb, pb = page(col.tobytes()), page(msk.tobytes()), page(pal.tobytes())
        st.map(IN_VA, cb, readonly=True)
        st.map(IN_VA + 0x400000, mb, readonly=True)
        st.map(IN_VA + 0x800000, pb, readonly=True)
        out = (C.c_uint32 * OUT_CAP)()
        st.map(OUT_VA, out)
        mode = theme | (0x100 if cell[0].startswith("hud_") else 0)   # the HUD panels: border-only (gfx_ui.h)
        n = st.call("holestats.uiCell", [IN_VA, IN_VA + 0x800000, IN_VA + 0x400000, w | h << 16, mode, scale, OUT_VA, OUT_CAP]).eax
        assert n > 0, cell[0]
        W, H = out[0], out[1]
        raw = bytes(out)
        px = np.frombuffer(raw[8:8 + 4 * W * H], dtype=np.uint8).reshape(H, W, 4).astype(np.float64)
        keep = np.frombuffer(raw[8 + 4 * W * H:8 + 4 * W * H + w * h], dtype=np.uint8).reshape(h, w).copy()
    return px, keep, col, msk, rgb


def over_art_alpha(px):
    a = px[..., 3:4] / 255.0
    return np.where(a > 0, px[..., :3] / np.maximum(a, 1e-6), 0)


def fidelity(lib, cell, theme=1):
    """the share of the art's opaque pixels (mask byte 0) the 1x render matches within 24 levels, allowing one pixel of offset"""
    px, keep, col, msk, rgb = cell_render(lib, cell, theme, 1)
    h, w = col.shape
    mine = over_art_alpha(px)
    opaque = (col != 0xFF) & (msk == 0) & (keep == 0) & (px[..., 3] >= 254)
    best = np.full((h, w), 1e9)
    pad = np.pad(rgb, ((1, 1), (1, 1), (0, 0)), mode="edge")
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            best = np.minimum(best, np.abs(mine - pad[1 + dy:h + 1 + dy, 1 + dx:w + 1 + dx]).max(axis=2))
    d = best[opaque]
    return float((d <= 24).mean()) if d.size else 1.0, int(opaque.sum())


def main(argv):
    from PIL import Image
    sys.path.insert(0, str(ROOT))
    from port.test import lib as L
    lib = L.Lib(os.environ["SG_PORTSIM_DLL"])
    out = pathlib.Path(argv[0])
    out.mkdir(parents=True, exist_ok=True)
    for cell in CELLS + HUD_CELLS:
        name = cell[0]
        g, keep, col, msk, rgb = cell_render(lib, cell, 1, 1)
        s, *_ = cell_render(lib, cell, 2, 1)
        art = np.where(((col != 0xFF) & (msk != 0xFF))[..., None], rgb, 255 * np.array([1.0, 0, 1.0]))
        shown = (keep == 2) & (col != 0xFF) & (msk != 0xFF)   # (P40E: only negctl 4 blits kept pixels after the scene)
        row = np.concatenate([art, np.where(shown[..., None], rgb, over_art_alpha(g)), np.where(shown[..., None], rgb, over_art_alpha(s))], axis=1)
        Image.fromarray(row.clip(0, 255).astype(np.uint8)).save(out / f"{name}.png")
        f, n = fidelity(lib, cell)
        print(f"{name}: fidelity {f:.3f} over {n} px")
    big, *_ = cell_render(lib, CELLS[0], 1, 3)
    Image.fromarray(over_art_alpha(big)[:330, :660].clip(0, 255).astype(np.uint8)).save(out / "finance_x3_crop.png")


if __name__ == "__main__":
    main(sys.argv[1:])
