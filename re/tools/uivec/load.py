"""uivec: the Load Game screen's buttons (P41): OK and Delete (bubbles in a socket ring) and Back (a matte disc).

Idle is the backdrop itself (Title_LoadGame: the buttons are painted into it); hover is a cell of Title_LoadGame_MO at the
same screen position (saveload.cpp / gfx_boot_screens.cpp: (0x258, 0x208, 0x46, 0x46) OK, (0x29e, 0x208, 0x46, 0x46)
Delete, (0x2e4, 0x212, 0x3c, 0x3c) Back). The flat boxes of this screen are boxes.py's.

Usage: py -3.12 re/tools/uivec/load.py [build | refine | style | accept | show]
"""
import copy
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import fit      # noqa: E402
import title    # noqa: E402

CELLS = {"ok": (0x258, 0x208, 0x46, 0x46), "delete": (0x29e, 0x208, 0x46, 0x46), "back": (0x2e4, 0x212, 0x3c, 0x3c)}
# rough centre and radius, read off an enlargement (log/roadmap/P41/load/btn_Title_LoadGame_MO.png)
HINTS = {"ok": (632.4, 556.0, 25.0), "delete": (705.2, 556.0, 21.4), "back": (768.5, 557.0, 22.0)}


def load():
    base_idx, base_pal = fit.load_sheet("Interface/Title_LoadGame.pcx")
    mo_idx, mo_pal = fit.load_sheet("Interface/Title_LoadGame_MO.pcx")
    hover = np.full_like(base_idx, 0xff)
    for x, y, w, h in CELLS.values():
        hover[y:y + h, x:x + w] = mo_idx[y:y + h, x:x + w]
    return base_idx, base_pal, base_idx, base_pal, hover, mo_pal


def same_place(meas):
    """On this screen a hover cell repaints its button where it already is (the title menu's hover bubbles sit a little
    apart, these do not), so the hover circle is the idle one and the hover glyph is traced inside it."""
    base_idx, base_pal, un_idx, un_pal, mo_idx, mo_pal = load()
    for name, m in meas.items():
        if m["kind"] != "bubble":
            continue
        cx, cy, r = m["idle"]["c"]
        m["hover"]["c"] = list(m["idle"]["c"])
        m["hover"]["body"] = list(m["idle"]["body"]) if m["idle"].get("body") else None
        m["hover"]["glyph"] = title.measure_glyph(mo_idx, mo_pal, CELLS[name], cx, cy, r)


DISABLED = {"ok": (0x258, 0x1b3), "delete": (0x29e, 0x1b3)}     # where the MO sheet keeps the grey cells


def load_disabled():
    """The same screen with the DISABLED cells standing in for the hover ones: the grey OK and Delete the game shows when
    no save is picked. Run as its own screen ("loaddis"), so the recipe's second state takes this grey paint."""
    base_idx, base_pal = fit.load_sheet("Interface/Title_LoadGame.pcx")
    mo_idx, mo_pal = fit.load_sheet("Interface/Title_LoadGame_MO.pcx")
    over = np.full_like(base_idx, 0xff)
    for n, (sx, sy) in DISABLED.items():
        x, y, w, h = CELLS[n]
        over[y:y + h, x:x + w] = mo_idx[sy:sy + h, sx:sx + w]
    return base_idx, base_pal, base_idx, base_pal, over, mo_pal


def setup_disabled():
    cells = {n: CELLS[n] for n in DISABLED}
    title.configure("loaddis", cells, load_disabled, hints=HINTS, post=same_place_dis)
    title.GLYPH_K = 0.86
    title.NEAR_K = 1.08
    title.PARTS_FORCE |= {"ok"}


def same_place_dis(meas):
    base_idx, base_pal, un_idx, un_pal, mo_idx, mo_pal = load_disabled()
    for name, m in meas.items():
        cx, cy, r = m["idle"]["c"]
        m["hover"]["c"] = list(m["idle"]["c"])
        m["hover"]["body"] = list(m["idle"]["body"]) if m["idle"].get("body") else None
        m["hover"]["glyph"] = title.measure_glyph(mo_idx, mo_pal, CELLS[name], cx, cy, r)


def setup():
    title.configure("load", CELLS, load, discs=("back",), hints=HINTS, post=same_place)
    title.GLYPH_K = 0.86
    title.PARTS_FORCE |= {"ok"}     # a clean check mark; its straight edges overlap the art's soft ones a little less
    title.NEAR_K = 1.08     # each bubble sits in a dark socket ring 4 pixels out


def main():
    dis = "--disabled" in sys.argv
    sys.argv = [a for a in sys.argv if a != "--disabled"]
    (setup_disabled if dis else setup)()
    what = sys.argv[1] if len(sys.argv) > 1 else "build"
    sys.argv = sys.argv[:1] + sys.argv[2:]
    if what == "build":
        title.build(3)
    elif what == "accept":
        import accept
        accept.main()
    elif what == "refine":
        import refine
        refine.KEEP_STYLE = True
        refine.main()
    elif what == "style":
        import accept
        import recipes
        b = accept.Bench()
        style = copy.deepcopy(recipes.STYLE)
        for hover in (False, True):
            accept.measure_bubble_style(b, style, hover, use_spot=True)
            if any(m["kind"] == "disc" for m in b.meas.values()):
                accept.measure_disc_style(b, style, hover)
        with open(title.STYLE_FILE, "w") as f:
            json.dump({"bubble": style["bubble"], "disc": style["disc"]}, f)
        recipes.STYLE.update(style)
        accept.main()
    elif what == "glyphs":
        import glyphs
        glyphs.main()
    elif what == "show":
        import inbutton
        from PIL import Image
        tiles = []
        for n in title.CELLS:
            p = os.path.join(fit.ROOT, "log", "roadmap", "P41", title.OUT, f"{n}_in_button.png")
            sys.argv = ["x", n, p]
            inbutton.main()
            tiles.append(Image.open(p))
        w, h = max(t.width for t in tiles), max(t.height for t in tiles)
        o = Image.new("RGB", (w * len(tiles), h))
        for k, t in enumerate(tiles):
            o.paste(t, (k * w, 0))
        o.save(os.path.join(fit.ROOT, "log", "roadmap", "P41", title.OUT, title.OUT + "_buttons.png"))


if __name__ == "__main__":
    main()
