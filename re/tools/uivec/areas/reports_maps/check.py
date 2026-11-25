"""The OK check of the report screens: a C1 bubble with a navy check mark, seated in a round socket (a navy ring around a
sunken lavender well) on Buy Land and the Routing map; the empty socket alone at the lower left of both screens.

The check mark (read 2026-11-24 from the coverage runs of the Buy Land hover cell, buy_land_buttons.pcx 0xc2, 0x7f; the
same glyph, moved, on the backdrop's blue check and the Routing map's yellow one): ONE outline. A long stroke leaning right
with a flat top (y 144.4, x 229.9..238.4), its right edge running down-left into a vertical right side of the foot
(x 228.1 from y 163); a short stroke at the left with a flat top (y 156.4, x 210.5..217.0) whose lower edge falls at
45 degrees and turns vertical into the foot (x ~217.5); a V notch between the strokes at (222.5, 161.9); one flat foot
(y 170.0, x 218.0..228.1).
"""
# the short stroke's lower edge is a smooth curve (row edges 210.7 .. 218.0 from y 157 to 169: steep, then turning down
# into the foot): read as five points (a first reading with two made a kink there)
CHECK = [[229.9, 144.4], [238.4, 144.4], [238.1, 146.2], [228.6, 162.0], [228.1, 163.4], [228.1, 170.0], [218.0, 170.0],
         [217.7, 168.0], [217.1, 165.5], [215.5, 162.5], [213.2, 159.5], [210.5, 156.4], [217.0, 156.4], [222.5, 161.9]]
CHECK_BODY = (224.99, 157.04)     # the body centre of the bubble the check was read in


def check_shape(dx=0.0, dy=0.0, pts=None):
    return {"t": "poly", "rings": [[[x + dx, y + dy] for x, y in (pts or CHECK)]]}


def socket(cx, cy, r, st):
    """A round socket: soft shade inside (a sunken well lit from the upper left: its paint measured as a bevel by depth and
    direction), a navy ring line of width st['w'] centred on radius r."""
    c = {"t": "circle", "c": [cx, cy], "r": r}
    L = [{"shape": c, "fill": {"bevel": {"L": r, "stops": st["stops"], "coef": st["coef"]}}},
         {"shape": {"t": "outline", "of": c, "w": st["w"]}, "fill": st["line"]}]
    return {"box": [cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4], "layers": L}
