"""The bulldozer bubble as cut for the terrain, elevation and amenities palettes: one glyph (el_bulldozer.py), placed on each
cut by matching its dark pixels against bulldozer-button idle (local offsets below), paint measured per cut."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import el_bulldozer as BD  # noqa: E402
from geom import poly  # noqa: E402

HOW = ("tan bubble, navy rim, on the palette's lavender corner; glyph: a bulldozer facing left (el_bulldozer.py: curved "
       "blade, push arm, yellow cab with a navy outline, low yellow engine with an exhaust stack, 2 px frame, track as a "
       "navy stadium with a lighter top run and two staggered rows of light tread dots)")
SPECS = [
    dict(id="bulldozer-button", ref="idle", circle=(15.5, 15.8, 14.0), shadow=True, glyph=BD.glyph(),
         ground=[("lavender corner of the tray (its top right cut by the tray's curved edge)",
                  poly((0, 0), (26, 0), (29, 3), (31, 4), (32.5, 6), (34, 6), (34, 38), (0, 38)))], how=HOW),
    dict(id="elevation-bulldozer", ref="idle", circle=(15.5, 16.8, 14.0), ground="auto", glyph=BD.glyph(0, 1), how=HOW),
    dict(id="amenities-bulldozer", ref="idle", circle=(14.5, 14.8, 14.0), ground="auto", glyph=BD.glyph(-1, -1), how=HOW),
]

if __name__ == "__main__":
    import bel
    want = sys.argv[1:]
    for sp in SPECS:
        if not want or sp["id"] in want:
            bel.build(sp)
