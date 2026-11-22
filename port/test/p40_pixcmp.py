"""P40D: pixel comparison of the procedural UI theme against the game's art (log/roadmap/P40/P40_notes.md, P40D).

For every element the theme replaces (the report cells of p40_cells.CELLS, the HUD panels, the Hole Stats screen, the OK button and
the popup / message frames) the same element is drawn twice over the same background, as the game draws it:
  * ART: the original's draw of the art cell (Surface_stretchBlit with its mask cell, or Image_draw's colour key) into the RGB555
    frame, expanded to 8 bits per channel the way the presenter expands a 555 word;
  * THEME: the theme's scene rendered at the frame's scale (scale 1: what the `ui` family writes into the 800 x 600 root) and at the
    window's scale (1.8 = a 1920 x 1080 window, 3.6 = 3840 x 2160: what the presenter's TextRaster draws), each pixel blended over the
    background word in 5 bits per channel as ui_over555 does, the kept art pixels drawn as the original draws them (at the window's
    scale they show the frame's pixel, as the run is not visible there), and the window render box-filtered (exact area weights)
    back to 800 x 600.
At 800 x 600 the art is 1:1. At 1080 / 4K the original shows the same art scaled up, and a window pixel straddling two frame pixels
mixes them in any picture at a non-integer scale (the art too), so the reference there is the art through the same pipeline: scaled
up area-exact (each window pixel the mean of the art's pixel blocks it covers: the port's sharp filter) and box-filtered back with the
same weights. A theme that redraws the art's shapes and colours exactly scores 0 against it at every scale. Per element and scale: mean and 95th percentile of the absolute difference per channel, CIEDE2000 delta E (mean, p95), the
share of pixels within 4 / 8 / 16 levels (largest channel difference), over the pixels either side covers.
    ~/bin/py -3.12 port/test/p40_pixcmp.py <out dir> [--theme 1] [--only name,...] [--tag before]
writes <out>/pixcmp.tsv (one row per element and scale, then one per screen and scale), <out>/<screen>.png (art | theme | delta E at
1x, then the delta E at 1080 and 4K), <out>/worst10.png (the 10 elements with the highest mean delta E at 1x: art scaled 3.6x nearest
| theme at 4K | delta E, cropped around each element's worst area) and <out>/<screen>_4k.png (art 3.6x nearest | theme at 4K).
Needs SG_PORTSIM_DLL and the user's install (original\\Interface); nothing of the art is stored.
"""
from __future__ import annotations

import ctypes as C
import math
import os
import pathlib
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from port.test import p40_cells as PC  # noqa: E402

IFACE = ROOT / "original" / "Interface"
INFO = IFACE / "infoscreens"
OUT_VA = 0x30000000
IN_VA = 0x34000000
OUT_CAP = 1 << 24
SCALES = (1.0, 1.8, 3.6)
BG = (0x40, 0x70, 0x40)   # the background behind every element (a course green); the same word under the art and the theme


def w555(rgb):
    r, g, b = (int(v) >> 3 for v in rgb)
    return r << 10 | g << 5 | b


def expand(word):
    """555 words -> 8-bit channels (rgb555_to_bgra: l << 3 | l >> 2)"""
    word = np.asarray(word, dtype=np.int32)
    ch = [(word >> s) & 31 for s in (10, 5, 0)]
    return np.stack([(c << 3) | (c >> 2) for c in ch], axis=-1).astype(np.float64)


def blit_words(dst, pal555, idx, msk):
    """Surface_stretchBlit's mask blend (gfx_ui.cpp ui_swap's kept pixels): k = 0 copies the colour word, else per channel
    (((dst & 0xf8) * k) >> 8) + (src & 0xf8), >> 3; pixels with index 0xff or mask 0xff are not drawn"""
    out = dst.copy()
    draw = (idx < 0xFF) & (msk < 0xFF)
    sw = pal555[idx].astype(np.int32)
    k = msk.astype(np.int32)
    res = np.zeros_like(out)
    for sh in (10, 5, 0):
        dv = ((out >> sh) & 31) << 3
        sv = ((sw >> sh) & 31) << 3
        c = (((dv * k) >> 8) + sv) & 0xFFFF
        res |= ((c >> 3) & 31) << sh
    res = np.where(k == 0, sw, res)
    out[draw] = res[draw]
    return out, draw


def over555(prem, bgword):
    """ui_over555 of premultiplied pixels (r, g, b 0..255, a 0..1) over the background words"""
    e = expand(bgword)
    v = prem[..., :3] + (1.0 - prem[..., 3:4]) * e
    q = np.floor(np.clip(v, 0, 255) * 31.0 / 255.0 + 0.5).astype(np.int32)   # lround of a non-negative value
    return (q[..., 0] << 10) | (q[..., 1] << 5) | q[..., 2]


# ------------------------------------------------------------------------------------------------ colour difference
def _lab(rgb):
    c = rgb / 255.0
    c = np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
    m = np.array([[0.4124564, 0.3575761, 0.1804375], [0.2126729, 0.7151522, 0.0721750], [0.0193339, 0.1191920, 0.9503041]])
    xyz = c @ m.T / np.array([0.95047, 1.0, 1.08883])
    f = np.where(xyz > (6 / 29) ** 3, np.cbrt(xyz), xyz / (3 * (6 / 29) ** 2) + 4 / 29)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], axis=-1)


def delta_e2000(a, b):
    """CIEDE2000 between two sRGB arrays (..., 3) of 0..255"""
    L1, a1, b1 = np.moveaxis(_lab(a), -1, 0)
    L2, a2, b2 = np.moveaxis(_lab(b), -1, 0)
    C1, C2 = np.hypot(a1, b1), np.hypot(a2, b2)
    Cm = (C1 + C2) / 2
    G = 0.5 * (1 - np.sqrt(Cm ** 7 / (Cm ** 7 + 25.0 ** 7)))
    a1p, a2p = (1 + G) * a1, (1 + G) * a2
    C1p, C2p = np.hypot(a1p, b1), np.hypot(a2p, b2)
    h1p = np.degrees(np.arctan2(b1, a1p)) % 360
    h2p = np.degrees(np.arctan2(b2, a2p)) % 360
    dLp, dCp = L2 - L1, C2p - C1p
    dh = h2p - h1p
    dh = np.where(dh > 180, dh - 360, np.where(dh < -180, dh + 360, dh))
    dh = np.where(C1p * C2p == 0, 0, dh)
    dHp = 2 * np.sqrt(C1p * C2p) * np.sin(np.radians(dh / 2))
    Lpm, Cpm = (L1 + L2) / 2, (C1p + C2p) / 2
    hs = h1p + h2p
    hpm = np.where(C1p * C2p == 0, hs, np.where(np.abs(h1p - h2p) <= 180, hs / 2, np.where(hs < 360, (hs + 360) / 2, (hs - 360) / 2)))
    T = (1 - 0.17 * np.cos(np.radians(hpm - 30)) + 0.24 * np.cos(np.radians(2 * hpm)) + 0.32 * np.cos(np.radians(3 * hpm + 6))
         - 0.20 * np.cos(np.radians(4 * hpm - 63)))
    dtheta = 30 * np.exp(-(((hpm - 275) / 25) ** 2))
    Rc = 2 * np.sqrt(Cpm ** 7 / (Cpm ** 7 + 25.0 ** 7))
    Sl = 1 + 0.015 * (Lpm - 50) ** 2 / np.sqrt(20 + (Lpm - 50) ** 2)
    Sc, Sh = 1 + 0.045 * Cpm, 1 + 0.015 * Cpm * T
    Rt = -np.sin(np.radians(2 * dtheta)) * Rc
    return np.sqrt((dLp / Sl) ** 2 + (dCp / Sc) ** 2 + (dHp / Sh) ** 2 + Rt * (dCp / Sc) * (dHp / Sh))


# ------------------------------------------------------------------------------------------------ the renders
def _page(b):
    n = (len(b) + 0xFFF) & ~0xFFF
    return (C.c_uint8 * n).from_buffer_copy(b.ljust(n, bytes(1)))


_MEMO: dict = {}   # the renders of one run (the same element, theme and scale are asked for several times); cleared by run()


def render_cell(lib, col, msk, pal555, theme, scale, border_only=False):
    """holestats.uiCell at `scale` (thousandths above 1000): premultiplied (H, W, 4) (alpha 0..1) and the kept mask"""
    key = ("cell", id(lib), col.tobytes(), msk.tobytes(), pal555.tobytes(), theme, scale, border_only)
    if key not in _MEMO:
        if len(_MEMO) >= 6:
            _MEMO.clear()
        _MEMO[key] = _render_cell(lib, col, msk, pal555, theme, scale, border_only)
    px, keep = _MEMO[key]
    return (None if px is None else px.copy()), keep.copy()


def _render_cell(lib, col, msk, pal555, theme, scale, border_only=False):
    h, w = col.shape
    with lib.state() as st:
        st.map(IN_VA, _page(np.ascontiguousarray(col).tobytes()), readonly=True)
        st.map(IN_VA + 0x400000, _page(np.ascontiguousarray(msk).tobytes()), readonly=True)
        st.map(IN_VA + 0x800000, _page(pal555.astype(np.uint16).tobytes()), readonly=True)
        out = (C.c_uint32 * OUT_CAP)()
        st.map(OUT_VA, out)
        sc = int(round(scale)) if abs(scale - round(scale)) < 1e-9 else int(round(scale * 1000))
        mode = theme | (0x100 if border_only else 0)
        n = st.call("holestats.uiCell", [IN_VA, IN_VA + 0x800000, IN_VA + 0x400000, w | h << 16, mode, sc, OUT_VA, OUT_CAP]).eax
        if n == 0:
            return None, np.zeros((h, w), np.uint8)
        W, H = out[0], out[1]
        raw = bytes(out)
        px = np.frombuffer(raw[8:8 + 4 * W * H], dtype=np.uint8).reshape(H, W, 4).astype(np.float64)
        keep = np.frombuffer(raw[8 + 4 * W * H:8 + 4 * W * H + w * h], dtype=np.uint8).reshape(h, w).copy()
    px[..., 3] /= 255.0
    return px, keep


def render_scene(lib, which, theme, x, y, hov, scale, box):
    """holestats.uiRender (a Hole Stats scene or the button) at `scale`, pasted into the frame box (x0, y0, w, h) at that scale"""
    bx, by, bw, bh = box
    W, H = int(math.ceil(bw * scale - 1e-6)), int(math.ceil(bh * scale - 1e-6))
    canvas = np.zeros((H, W, 4))
    with lib.state() as st:
        out = (C.c_uint32 * OUT_CAP)()
        st.map(OUT_VA, out)
        sc = int(round(scale)) if abs(scale - round(scale)) < 1e-9 else int(round(scale * 1000))
        n = st.call("holestats.uiRender", [which, theme, x, y, hov, sc, OUT_VA, OUT_CAP]).eax
        assert n > 4, (which, theme, scale)
        w, h, x0, y0 = out[0], out[1], out[2], out[3]
        px = np.frombuffer(bytes(out)[16:16 + 4 * w * h], dtype=np.uint8).reshape(h, w, 4).astype(np.float64)
    px[..., 3] /= 255.0
    ox, oy = int(round((x0 - bx) * scale)), int(round((y0 - by) * scale))
    ya, xa = max(0, oy), max(0, ox)
    yb, xb = min(H, oy + h), min(W, ox + w)
    if yb > ya and xb > xa:
        canvas[ya:yb, xa:xb] = px[ya - oy:yb - oy, xa - ox:xb - ox]
    return canvas


def area_matrix(n, scale):
    """(n, N) weights: frame pixel x is the mean of the window pixels over [x s, (x + 1) s)"""
    N = int(math.ceil(n * scale - 1e-6))
    A = np.zeros((n, N))
    for x in range(n):
        a, b = x * scale, (x + 1) * scale
        for i in range(int(math.floor(a)), min(N, int(math.ceil(b)))):
            A[x, i] = max(0.0, min(b, i + 1) - max(a, i))
        A[x] /= A[x].sum()
    return A


_AREA: dict = {}


def upsample(img, n_h, n_w, scale):
    """the frame picture (n_h, n_w, c) at the window's scale, area-exact (the transpose of the box filter's weights, rows of 1)"""
    for key, n in (((n_h, scale), n_h), ((n_w, scale), n_w)):
        if key not in _AREA:
            _AREA[key] = area_matrix(n, scale)
    Bh, Bw = _AREA[(n_h, scale)].T * scale, _AREA[(n_w, scale)].T * scale
    # the last window pixel reaches past the picture's edge: the mean of the part inside (rows of 1)
    Bh, Bw = Bh / Bh.sum(axis=1, keepdims=True), Bw / Bw.sum(axis=1, keepdims=True)
    return np.stack([Bh @ img[..., c] @ Bw.T for c in range(img.shape[2])], axis=-1)


def reference(art, scale):
    """the art as the window at `scale` shows it, box-filtered back to the frame (the art itself at scale 1)"""
    if scale == 1.0:
        return art
    h, w = art.shape[:2]
    return downsample(upsample(art, h, w, scale), h, w, scale)


def downsample(img, n_h, n_w, scale):
    key_h, key_w = (n_h, scale), (n_w, scale)
    if key_h not in _AREA:
        _AREA[key_h] = area_matrix(n_h, scale)
    if key_w not in _AREA:
        _AREA[key_w] = area_matrix(n_w, scale)
    Ah, Aw = _AREA[key_h], _AREA[key_w]
    img = img[:Ah.shape[1], :Aw.shape[1]]
    return np.stack([Ah @ img[..., c] @ Aw.T for c in range(img.shape[2])], axis=-1)


# ------------------------------------------------------------------------------------------------ the elements
class Element:
    """name, screen, the art (index, mask, 555 palette; mask None = Image_draw's colour key), where it sits on the screen canvas,
    and how the theme renders it"""

    def __init__(self, name, screen, idx, msk, pal, pos, kind="cell", scene=None, border_only=False):
        self.name, self.screen, self.idx, self.pal, self.pos, self.kind = name, screen, idx, pal, pos, kind
        self.msk = msk if msk is not None else np.where(idx >= 0xFE, 0xFF, 0).astype(np.uint8)
        if msk is None:
            self.idx = np.where(idx >= 0xFE, 0xFF, idx).astype(np.uint8)
        self.scene, self.border_only = scene, border_only
        self.h, self.w = idx.shape

    def art(self):
        bgw = np.full((self.h, self.w), w555(BG), np.int32)
        words, drawn = blit_words(bgw, self.pal, self.idx, self.msk)
        return words, drawn

    def theme(self, lib, theme, scale):
        """the theme's picture at 800 x 600 (the window render box-filtered when scale > 1) and its cover"""
        bgw = np.full((self.h, self.w), w555(BG), np.int32)
        if self.kind == "cell":
            px, keep = render_cell(lib, self.idx, self.msk, self.pal, theme, scale, self.border_only)
            if px is None:   # an empty cell: the art as it is
                return expand(self.art()[0]), self.art()[1]
        else:
            which, x, y, hov = self.scene
            px = render_scene(lib, which, theme, x, y, hov, scale, (x if which == 3 else 0, y if which != 0 else 0, self.w, self.h))
            keep = np.zeros((self.h, self.w), np.uint8)
        H, W = px.shape[:2]
        # the scale-1 frame: the scene over the background, then the kept pixels from the art
        if scale == 1.0:
            one = px[:self.h, :self.w]
            cov1 = one[..., 3] > 0
            fw = over555(one, bgw)
        else:
            one_px, _ = (render_cell(lib, self.idx, self.msk, self.pal, theme, 1.0, self.border_only) if self.kind == "cell"
                         else (render_scene(lib, self.scene[0], theme, self.scene[1], self.scene[2], self.scene[3], 1.0,
                                            (self.scene[1] if self.scene[0] == 3 else 0, self.scene[2] if self.scene[0] != 0 else 0,
                                             self.w, self.h)), None))
            cov1 = one_px[..., 3] > 0
            fw = over555(one_px, bgw)
        kept = keep.astype(bool) & (self.idx < 0xFF) & (self.msk < 0xFF)
        fw, _ = blit_words(fw, self.pal, np.where(kept, self.idx, 0xFF).astype(np.uint8), self.msk)
        cover = cov1 | kept
        if scale == 1.0:
            return expand(fw), cover
        # the window: the scene over the background word in 5 bits; where the frame pixel is kept, the frame's pixel (nearest)
        yy = np.minimum((np.arange(H) / scale).astype(int), self.h - 1)
        xx = np.minimum((np.arange(W) / scale).astype(int), self.w - 1)
        win = expand(over555(px, np.full((H, W), w555(BG), np.int32)))
        kup = kept[yy][:, xx]
        if kup.any():   # the kept pixels show the frame, scaled up as the reference is
            win = np.where(kup[..., None], upsample(expand(fw), self.h, self.w, scale)[:H, :W], win)
        return downsample(win, self.h, self.w, scale), cover


def _pcx(name):
    from PIL import Image
    p = INFO / f"{name}.pcx" if (INFO / f"{name}.pcx").exists() else IFACE / f"{name}.pcx"
    im = Image.open(p)
    pal = im.getpalette()
    p555 = np.array([((pal[3 * i] >> 3) << 10) | ((pal[3 * i + 1] >> 3) << 5) | (pal[3 * i + 2] >> 3) for i in range(256)], np.uint16)
    return np.asarray(im, dtype=np.uint8), p555


SCREEN_OF = {"finance": "finance", "course": "course", "sga": "sga", "comments": "comments", "roster": "roster",
             "lowscore": "lowscore", "histograph": "histograph", "hire": "hire", "tour": "tournament", "eoy": "endofyear",
             "shortcuts": "shortcuts", "route": "routing", "buy": "buyland", "hud": "hud"}


def _frame(kind, w, h):
    """drawPanelS2 (kind 0: InfoButtons cells 30.. at x 0xc8 + 0x11 c, y 0x11 r, interior 0x4e79) or drawFrameR3 (kind 1: the
    TransPopups 3 x 3 grid at 0x11) assembled at (w, h) as gfx_ui.cpp frame_drawer lays the pieces out"""
    if kind == 0:
        img, pal = _pcx("InfoButtons")
        al, _ = _pcx("InfoButtons_A")
        ox = 0xC8
    else:
        img, pal = _pcx("TransPopups")
        al, _ = _pcx("TransPopups_A")
        ox = 0
    def cell(k):   # grid cell k = 3 r + c
        r, c = divmod(k, 3)
        x, y = ox + 0x11 * c, 0x11 * r
        return img[y:y + 16, x:x + 16], al[y:y + 16, x:x + 16]
    kcell = [0, 1, 7, 3, 5, 2, 8, 6]   # top-left, top, bottom, left, right, top-right, bottom-right, bottom-left
    idx = np.full((h, w), 0xFF, np.uint8)
    msk = np.full((h, w), 0xFF, np.uint8)
    pal = pal.copy()
    if kind == 0:
        used = set(np.unique(np.concatenate([cell(k)[0].ravel() for k in kcell])).tolist())
        slot = next(v for v in range(0xFE, -1, -1) if v not in used)
        pal[slot] = 0x4E79
        idx[4:h - 4, 4:w - 4] = slot
        msk[4:h - 4, 4:w - 4] = 0
    def piece(k, px, py):
        ci, cm = cell(kcell[k])
        for yy in range(16):
            for xx in range(16):
                X, Y = px + xx, py + yy
                if 0 <= X < w and 0 <= Y < h and ci[yy, xx] < 0xFF and cm[yy, xx] < 0xFF:
                    idx[Y, X], msk[Y, X] = ci[yy, xx], cm[yy, xx]
    piece(0, 0, 0)
    if w - 16 > 16:
        for c in range((w - 16 - 1) >> 4):
            piece(1, 16 + 16 * c, 0)
            piece(2, 16 + 16 * c, h - 16)
    if h - 16 > 16:
        for c in range((h - 16 - 1) >> 4):
            piece(3, 0, 16 + 16 * c)
            piece(4, w - 16, 16 + 16 * c)
    piece(5, w - 16, 0)
    piece(6, w - 16, h - 16)
    piece(7, 0, h - 16)
    return idx, msk, pal


def elements(legacy=False):
    """legacy: the Hole Stats screen and the OK button as P40's hand-made scenes (what the game theme drew before P40D); else as the
    game theme draws them since P40D (the cell engine over the HoleSTAT cuts and the OkStates cells)"""
    out = []
    for cell in PC.CELLS:
        name, sheet, alpha, x, y, w, h = cell
        col, msk, p555, _ = PC.cell_bytes(sheet, alpha, x, y, w, h)
        out.append(Element(name, SCREEN_OF[name.split("_")[0]], col, msk if alpha else None, p555, (x, y)))
    yo = 0
    for cell in PC.HUD_CELLS:
        name, sheet, alpha, x, y, w, h = cell
        col, msk, p555, _ = PC.cell_bytes(sheet, alpha, x, y, w, h)
        out.append(Element(name, "hud", col, msk, p555, (0, yo), border_only=True))
        yo += h + 4
    # the Hole Stats screen (P40's scenes; gfx_routing.cpp's HoleSTAT cuts: head 0, 0, 800 x 220; row 0, 0x11a, 800 x 16; foot
    # 0, 0x15b, 800 x 55)
    hs, hp = _pcx("HoleSTAT")
    ha, _ = _pcx("HoleSTAT_alpha")
    for name, which, y, hh in (("hs_head", 0, 0, 220), ("hs_row", 1, 0x11A, 16), ("hs_foot", 2, 0x15B, 55)):
        if legacy:
            out.append(Element(name, "holestats", hs[y:y + hh], ha[y:y + hh], hp, (0, y), kind="scene", scene=(which, 0, y, 0)))
        else:
            out.append(Element(name, "holestats", hs[y:y + hh], ha[y:y + hh], hp, (0, y)))
    # the OK button (OkStates cells 0 and 1 at (1, 1) and (0x2e, 1), 44 x 42, Image_draw: indexes 0xfe / 0xff transparent)
    ok, op = _pcx("OkStates")
    kw = {"kind": "scene"} if legacy else {}
    out.append(Element("ok_button", "popups", ok[1:43, 1:45], None, op, (8, 8), scene=(3, 0, 0, 0), **kw))
    out.append(Element("ok_button_hover", "popups", ok[1:43, 0x2E:0x2E + 44], None, op, (60, 8), scene=(3, 0, 0, 1), **kw))
    for name, kind, w, h, pos in (("popup_panel", 0, 320, 192, (8, 60)), ("message_frame", 1, 384, 160, (340, 60))):
        idx, msk, pal = _frame(kind, w, h)
        out.append(Element(name, "popups", idx, msk, pal, pos))
    return out


# ------------------------------------------------------------------------------------------------ the measures
def measure(art, mine, mask):
    d = np.abs(mine - art)[mask]
    if d.size == 0:
        return None
    mx = d.max(axis=1)
    de = delta_e2000(art[mask], mine[mask])
    return {"n": int(mask.sum()), "mean_r": d[:, 0].mean(), "mean_g": d[:, 1].mean(), "mean_b": d[:, 2].mean(),
            "p95_r": np.percentile(d[:, 0], 95), "p95_g": np.percentile(d[:, 1], 95), "p95_b": np.percentile(d[:, 2], 95),
            "de_mean": de.mean(), "de_p95": np.percentile(de, 95), "w4": (mx <= 4).mean(), "w8": (mx <= 8).mean(),
            "w16": (mx <= 16).mean()}


COLS = ["n", "mean_r", "mean_g", "mean_b", "p95_r", "p95_g", "p95_b", "de_mean", "de_p95", "w4", "w8", "w16"]


def heat(de):
    """delta E -> colour: black (0) .. red (5) .. yellow (10) .. white (20 and over)"""
    t = np.clip(de / 20.0, 0, 1)
    r = np.clip(t * 4, 0, 1)
    g = np.clip(t * 4 - 1, 0, 1)
    b = np.clip(t * 2 - 1, 0, 1)
    return (np.stack([r, g, b], -1) * 255).astype(np.uint8)


def grid_info(lib, el, theme):
    """the cell scene's elements by colour grid step: 'n elements; grid step s:count ...' (holestats.uiCellInfo)"""
    if el.kind != "cell":
        return "hand scene"
    if not lib.has("holestats.uiCellInfo"):
        return ""
    with lib.state() as st:
        st.map(IN_VA, _page(np.ascontiguousarray(el.idx).tobytes()), readonly=True)
        st.map(IN_VA + 0x400000, _page(np.ascontiguousarray(el.msk).tobytes()), readonly=True)
        st.map(IN_VA + 0x800000, _page(el.pal.astype(np.uint16).tobytes()), readonly=True)
        out = (C.c_uint32 * 0x4000)()
        st.map(OUT_VA, out)
        n = st.call("holestats.uiCellInfo", [IN_VA, IN_VA + 0x800000, IN_VA + 0x400000, el.w | el.h << 16,
                                             theme | (0x100 if el.border_only else 0), OUT_VA, 0x4000]).eax
        if n == 0:
            return "empty"
        k = out[0]
        steps = {}
        for i in range(k):
            steps[out[2 + 3 * i]] = steps.get(out[2 + 3 * i], 0) + 1
    return f"{k} elements; grid step " + " ".join(f"{s_}:{c}" for s_, c in sorted(steps.items()))


def run(lib, out, theme=1, only=None, tag="", legacy=False):
    from PIL import Image
    out = pathlib.Path(out)
    out.mkdir(parents=True, exist_ok=True)
    rows, screens = [], {}
    _MEMO.clear()
    for el in elements(legacy):
        if only and el.name not in only and el.screen not in only:
            continue
        info = grid_info(lib, el, theme)
        words, drawn = el.art()
        art = expand(words)
        rec = {"el": el, "art": art}
        for s in SCALES:
            mine, cover = el.theme(lib, theme, s)
            ref = reference(art, s)
            mask = drawn | cover
            m = measure(ref, mine, mask)
            de = np.zeros(mask.shape)
            de[mask] = delta_e2000(ref[mask], mine[mask])
            rec[s] = (mine, mask, de, ref)
            if m:
                m["grid"] = info
                rows.append((el.name, el.screen, s, m))
                print(f"{el.name:18s} x{s:<4} w8 {m['w8']:.3f} dE {m['de_mean']:.2f} p95 {m['de_p95']:.1f}  w4 {m['w4']:.3f} w16 {m['w16']:.3f}",
                      flush=True)
        screens.setdefault(el.screen, []).append(rec)
    # per screen: the pixels of its elements together
    for scr, recs in screens.items():
        for s in SCALES:
            A = np.concatenate([r[s][3][r[s][1]] for r in recs])
            M = np.concatenate([r[s][0][r[s][1]] for r in recs])
            m = measure(A[:, None, :], M[:, None, :], np.ones((len(A), 1), bool))
            m["grid"] = ""
            rows.append(("*", scr, s, m))
    with open(out / "pixcmp.tsv", "w", encoding="utf-8") as f:
        # target: at least 97 % of the pixels within 8 levels and a mean delta E under 2
        f.write("element\tscreen\tscale\t" + "\t".join(COLS) + "\ttarget\tscene\n")
        for name, scr, s, m in rows:
            ok = "PASS" if m["w8"] >= 0.97 and m["de_mean"] < 2.0 else "FAIL"
            f.write(f"{name}\t{scr}\t{s}\t" + "\t".join(f"{m[c]:.4f}" if c != "n" else str(m[c]) for c in COLS) + f"\t{ok}\t{m['grid']}\n")
    # pictures per screen
    for scr, recs in screens.items():
        W = max(r["el"].pos[0] + r["el"].w for r in recs)
        H = max(r["el"].pos[1] + r["el"].h for r in recs)
        def canvas(fn):
            c = np.zeros((H, W, 3), np.uint8)
            c[...] = (40, 40, 48)
            for r in recs:
                x, y = r["el"].pos
                v, m = fn(r)
                c[y:y + r["el"].h, x:x + r["el"].w][m] = v[m]
            return c
        art_c = canvas(lambda r: (np.clip(r["art"], 0, 255).astype(np.uint8), r[1.0][1]))
        mine_c = canvas(lambda r: (np.clip(r[1.0][0], 0, 255).astype(np.uint8), r[1.0][1]))
        heats = [canvas(lambda r, s=s: (heat(r[s][2]), r[s][1])) for s in SCALES]
        top = np.concatenate([art_c, mine_c, heats[0]], axis=1)
        if len(heats) == 3:
            bot = np.concatenate([heats[1], heats[2], np.zeros_like(art_c)], axis=1)
            top = np.concatenate([top, bot], axis=0)
        Image.fromarray(top).save(out / f"{scr}.png")
        # the 4K side-by-side of the screen: art 3.6x nearest | theme at 4K (its window render, not downsampled)
        if W * H <= 800 * 620 and 3.6 in SCALES:
            s = 3.6
            Wn, Hn = int(math.ceil(W * s)), int(math.ceil(H * s))
            yy = np.minimum((np.arange(Hn) / s).astype(int), H - 1)
            xx = np.minimum((np.arange(Wn) / s).astype(int), W - 1)
            left = art_c[yy][:, xx]
            right = np.zeros_like(left)
            right[...] = (40, 40, 48)
            for r in recs:
                el = r["el"]
                win = window_picture(lib, el, theme, s)
                x0, y0 = int(round(el.pos[0] * s)), int(round(el.pos[1] * s))
                hh, ww = min(win.shape[0], Hn - y0), min(win.shape[1], Wn - x0)
                cov = r[1.0][1][np.minimum((np.arange(hh) / s).astype(int), el.h - 1)][:, np.minimum((np.arange(ww) / s).astype(int), el.w - 1)]
                right[y0:y0 + hh, x0:x0 + ww][cov] = win[:hh, :ww][cov]
            Image.fromarray(np.concatenate([left, right], axis=1)).save(out / f"{scr}_4k.png")
    # the worst 10 elements at 1x: crops around the worst area at 4K
    worst = sorted([r for r in rows if r[0] != "*" and r[2] == 1.0], key=lambda r: -r[3]["de_mean"])[:10] if 3.6 in SCALES else []
    tiles = []
    allrec = {r["el"].name: r for recs in screens.values() for r in recs}
    for name, scr, _, m in worst:
        r = allrec[name]
        el = r["el"]
        de = r[1.0][2]
        cw, ch = min(el.w, 160), min(el.h, 90)
        # the crop window with the largest summed delta E
        ii = np.cumsum(np.cumsum(np.pad(de, ((1, 0), (1, 0))), 0), 1)
        sums = ii[ch:, cw:] - ii[:-ch, cw:] - ii[ch:, :-cw] + ii[:-ch, :-cw]
        cy, cx = np.unravel_index(np.argmax(sums), sums.shape)
        s = 3.6
        yy = np.minimum(cy + (np.arange(int(ch * s)) / s).astype(int), el.h - 1)
        xx = np.minimum(cx + (np.arange(int(cw * s)) / s).astype(int), el.w - 1)
        a4 = np.clip(r["art"], 0, 255).astype(np.uint8)[yy][:, xx]
        win = window_picture(lib, el, theme, s)
        t4 = win[int(cy * s):int(cy * s) + a4.shape[0], int(cx * s):int(cx * s) + a4.shape[1]]
        t4 = np.pad(t4, ((0, a4.shape[0] - t4.shape[0]), (0, a4.shape[1] - t4.shape[1]), (0, 0)))
        h4 = heat(de)[yy][:, xx]
        tile = np.concatenate([a4, t4, h4], axis=1)
        tile = np.pad(tile, ((0, 324 - tile.shape[0]), (0, 1728 - tile.shape[1]), (0, 0)))
        _label(tile, f"{name} x{cx},{cy} dE {m['de_mean']:.2f} w8 {m['w8']:.3f}")
        tiles.append(tile)
    if tiles:
        Image.fromarray(np.concatenate(tiles, axis=0)).save(out / "worst10.png")
    return rows


def window_picture(lib, el, theme, s):
    """the theme's window render at scale s as the player sees it (8-bit, over the background, kept pixels from the frame)"""
    bgw = np.full((el.h, el.w), w555(BG), np.int32)
    if el.kind == "cell":
        px, keep = render_cell(lib, el.idx, el.msk, el.pal, theme, s, el.border_only)
        one, _ = render_cell(lib, el.idx, el.msk, el.pal, theme, 1.0, el.border_only)
        if px is None:
            return np.zeros((int(math.ceil(el.h * s)), int(math.ceil(el.w * s)), 3), np.uint8)
    else:
        which, x, y, hov = el.scene
        box = (x if which == 3 else 0, y if which != 0 else 0, el.w, el.h)
        px = render_scene(lib, which, theme, x, y, hov, s, box)
        one = render_scene(lib, which, theme, x, y, hov, 1.0, box)
        keep = np.zeros((el.h, el.w), np.uint8)
    kept = keep.astype(bool) & (el.idx < 0xFF) & (el.msk < 0xFF)
    fw, _ = blit_words(over555(one, bgw), el.pal, np.where(kept, el.idx, 0xFF).astype(np.uint8), el.msk)
    H, W = px.shape[:2]
    yy = np.minimum((np.arange(H) / s).astype(int), el.h - 1)
    xx = np.minimum((np.arange(W) / s).astype(int), el.w - 1)
    win = np.where(kept[yy][:, xx], fw[yy][:, xx], over555(px, np.full((H, W), w555(BG), np.int32)))
    return np.clip(expand(win), 0, 255).astype(np.uint8)


def _label(tile, text):
    """a plain label strip (PIL's default bitmap font; tool output only)"""
    from PIL import Image, ImageDraw
    im = Image.fromarray(tile)
    ImageDraw.Draw(im).rectangle([0, 0, 8 * len(text) + 8, 14], fill=(0, 0, 0))
    ImageDraw.Draw(im).text((4, 2), text, fill=(255, 255, 255))
    tile[...] = np.asarray(im)


def main(argv):
    import argparse
    from port.test import lib as L
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--theme", type=int, default=1)
    ap.add_argument("--only", default="")
    ap.add_argument("--tag", default="")
    ap.add_argument("--scales", default="1,1.8,3.6", help="the scales measured (the pictures at 4K need 3.6)")
    ap.add_argument("--tune", default="", help="k=v,...: holestats.uiTune(k, v) first (gfx_ui.h ui_tune: 0 edge margin, 1 grid step, 2 smoothing)")
    ap.add_argument("--legacy", action="store_true", help="the Hole Stats screen and the OK button as P40's hand scenes (before P40D)")
    a = ap.parse_args(argv)
    global SCALES
    SCALES = tuple(float(v) for v in a.scales.split(","))
    lib = L.Lib(os.environ["SG_PORTSIM_DLL"])
    for kv in filter(None, a.tune.split(",")):
        k, v = kv.split("=")
        with lib.state() as st:
            st.call("holestats.uiTune", [int(k), int(round(float(v) * 1000)) & 0xFFFFFFFF])
    run(lib, a.out, a.theme, set(filter(None, a.only.split(","))), a.tag, a.legacy)


if __name__ == "__main__":
    main(sys.argv[1:])
