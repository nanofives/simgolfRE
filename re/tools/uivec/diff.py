"""uivec: the difficulty screen (P41). Four pads, each with a face bubble at its left end.

The idle state is the backdrop itself (TitleSelDiffUnSel: pads and blue faces are painted into it); the hover state is a
cell of TitleSelDiffMO drawn at the screen point the game's table gives (selectDifficultyScreen.redraw,
port/src/sim/gfx_boot_screens.cpp: x 0xc1, 0x96, 0xa1, 0xc8; y 0x20, 0xa1, 0x140, 0x1b4).

The LOOK is the title menu's (style.json, measured there): this screen only measures its own shapes, so its acceptance
table says how well the recipes carry over to art they were not fitted on.

Usage: py -3.12 re/tools/uivec/diff.py [build | accept | refine]
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import fit      # noqa: E402
import title    # noqa: E402

# name: (the hover cell's cut in the MO sheet, where the game draws it)
PLACED = {"easy": ((0, 0, 0x152, 0x92), (0xc1, 0x20)), "medium": ((400, 0, 0x152, 0x92), (0x96, 0xa1)),
          "hard": ((0, 0x12c, 0x14c, 0x72), (0xa1, 0x140)), "expert": ((400, 0x12c, 0x14c, 0x82), (0xc8, 0x1b4))}
CELLS = {n: (X, Y, cut[2], cut[3]) for n, (cut, (X, Y)) in PLACED.items()}
# rough centre and radius of each face bubble, read off a gridded overview (log/roadmap/P41/diff/overview.png)
HINTS = {"easy": (243.5, 83.5, 48.0), "medium": (202.5, 217.5, 47.0), "hard": (211.0, 372.5, 47.0),
         "expert": (252.5, 506.0, 47.0)}


def load():
    base_idx, base_pal = fit.load_sheet("Interface/TitleSelDiffUnSel.pcx")
    mo_idx, mo_pal = fit.load_sheet("Interface/TitleSelDiffMO.pcx")
    hover = np.full_like(base_idx, 0xff)
    for cut, (X, Y) in PLACED.values():
        sx, sy, w, h = cut
        hover[Y:Y + h, X:X + w] = mo_idx[sy:sy + h, sx:sx + w]
    # idle: nothing is drawn over the backdrop, so the "overlay" is the backdrop
    return base_idx, base_pal, base_idx, base_pal, hover, mo_pal


def pads_from_hover(meas):
    """This screen's idle wells are almost white inside a cream glow, and the cream test finds the glow, not the well. The
    hover cell's yellow well is unmistakable, so the pad is taken from it: the idle well is the hover well moved back with
    its bubble, and the ring starts from the proportions the title menu's pads share, mirrored (the bubble sits at the
    left end here, and the ring is wide on the bubble's side). refine then moves both to the art."""
    for m in meas.values():
        hw = m["hover"]["inner"]
        dx, dy = m["idle"]["c"][0] - m["hover"]["c"][0], m["idle"]["c"][1] - m["hover"]["c"][1]
        ho = [round(hw[0] - 0.134 * hw[2], 2), round(hw[1] + 1.0, 2), round(1.41 * hw[2], 2), round(1.37 * hw[3], 2), hw[4]]
        m["hover"]["outer"] = ho
        m["idle"]["inner"] = [round(hw[0] + dx, 2), round(hw[1] + dy, 2)] + list(hw[2:])
        m["idle"]["outer"] = [round(ho[0] + dx, 2), round(ho[1] + dy, 2)] + ho[2:]
        m["inner"], m["outer"] = m["idle"]["inner"], m["idle"]["outer"]


def setup():
    title.configure("diff", CELLS, load, hints=HINTS, post=pads_from_hover)
    title.GLYPH_K = 0.84    # these bodies darken toward the rim: the glyph is looked for well inside
    # the four faces are drawn from their parts: Mariano approved their look on 2026-11-24 ("it's good enough, let's move
    # on"), thin strokes overlapping the art less than a trace does
    title.PARTS_FORCE |= {"easy", "medium", "hard", "expert"}


if __name__ == "__main__":
    setup()
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
    elif what == "glyphs":
        import glyphs
        glyphs.main()
    elif what == "retrace":
        # the glyphs again, looked for well inside the bubble (its body darkens toward the rim and read as glyph)
        import json
        title.GLYPH_K = 0.84
        base_idx, base_pal, un_idx, un_pal, mo_idx, mo_pal = load()
        with open(title.GEOM) as f:
            meas = json.load(f)
        for name, m in meas.items():
            for st, (idx, pal) in (("idle", (un_idx, un_pal)), ("hover", (mo_idx, mo_pal))):
                cx, cy, r = m[st]["c"]
                m[st]["glyph"] = title.measure_glyph(idx, pal, CELLS[name], cx, cy, r)
                print(name, st, "rings", len(m[st]["glyph"]))
        with open(title.GEOM, "w") as f:
            json.dump(meas, f)
    elif what == "style":
        # this screen's own paint for the shared recipes, measured on its refined shapes
        import copy
        import json
        import accept
        import recipes
        b = accept.Bench()
        style = copy.deepcopy(recipes.STYLE)
        for hover in (False, True):
            accept.measure_bubble_style(b, style, hover, use_spot=True)
            accept.measure_pad_style(b, style, hover)
        with open(title.STYLE_FILE, "w") as f:
            json.dump({"bubble": style["bubble"], "pad": style["pad"]}, f)
        recipes.STYLE.update(style)
        accept.main()
