"""gb-mood: GBUBBLES' five 15 px mood faces (furious, unhappy, neutral, happy, delighted), one shape set per state.

Read with cmap.py on each cell, 2026-11-24 (local to each trimmed 15 x 15 cut). Every face is a disc with a darker rim, two
eyes and a mouth; the devil adds horns and a grin of teeth, the delighted face an open mouth of white teeth.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import rect, line, poly, circle, oval, U  # noqa: E402

FACE = [("face: darker rim", circle(7.5, 7.5, 7.5), "rim"), ("face", circle(7.5, 7.6, 6.4), "face")]

PARTS = {
    "furious": [("horn, left", poly((1.4, 3.0), (2.4, -0.2), (4.4, 2.0)), "rim"),
                ("horn, right", poly((10.6, 2.0), (12.6, -0.2), (13.6, 3.0)), "rim")] + FACE + [
        ("left eye: an angry bar slanting down to the nose", line((2.2, 4.4), (5.4, 6.6), w=1.7), "dark"),
        ("right eye: an angry bar slanting down to the nose", line((12.8, 4.4), (9.6, 6.6), w=1.7), "dark"),
        ("eye glints", U(rect(4, 6, 5, 7), rect(10, 6, 11, 7)), "grey"),
        ("grin: a dark mouth", rect(3.0, 8.0, 12.4, 12.0, 1.2), "mouth"),
        ("grin: grey teeth line", rect(4, 9, 11.4, 11), "grey"),
        ("grin: white teeth", U(rect(5, 9, 6, 11), rect(7, 9, 8, 11), rect(9, 9, 10, 11), rect(11, 10, 11.6, 11)), "white")],
    "unhappy": FACE + [
        ("left eye", rect(4, 3, 7, 6, 0.6), "dark"), ("right eye", rect(9, 3, 12, 6, 0.6), "dark"),
        ("frown: an arc bending down at both ends", line((3.5, 11.5), (5.2, 9.4), (7.5, 8.5), (9.8, 9.4), (12.5, 11.5), w=1.0, smooth=2), "dark"),
        ("lower lip: a short olive line", rect(6, 10, 10, 11), "lip")],
    "neutral": FACE + [
        ("left eye", rect(3.6, 4.0, 6.4, 7.0, 0.8), "dark"), ("right eye", rect(9.6, 4.0, 12.4, 7.0, 0.8), "dark"),
        ("eye highlights above and below", U(rect(4, 3, 6, 4), rect(4, 7, 6, 8), rect(10, 3, 12, 4), rect(10, 7, 12, 8)), "glint"),
        ("cheek light", rect(5, 8, 11, 9), "light"),
        ("mouth: a straight line", rect(3, 10, 13, 11), "mouth")],
    "happy": FACE + [
        ("left eye", rect(3.0, 3.6, 6.0, 6.6, 0.8), "dark"), ("right eye", rect(9.0, 3.6, 12.0, 6.6, 0.8), "dark"),
        ("smile: an arc rising at both ends", line((2.4, 9.2), (4.6, 10.6), (7.5, 11.5), (10.4, 10.6), (12.6, 9.2), w=1.2, smooth=2), "dark")],
    "delighted": FACE + [
        ("left eye", rect(3.0, 2.6, 6.0, 7.0, 0.9), "dark"), ("right eye", rect(9.0, 2.6, 12.0, 7.0, 0.9), "dark"),
        ("eye glints", U(rect(4, 4, 5, 5), rect(10, 4, 11, 5)), "grey"),
        ("open mouth: dark, wide at the top and round below", poly((2.0, 9.0), (4.0, 8.0), (11.0, 8.0), (13.0, 9.0), (12.2, 11.0),
                                                                   (10.2, 12.8), (4.8, 12.8), (2.8, 11.0), smooth=1), "dark"),
        ("open mouth: white teeth", oval(7.3, 10.4, 3.4, 1.9), "white")],
}
SPEC = dict(id="gb-mood", parts=PARTS["neutral"], parts_by_state=PARTS,
            how="five 15 px faces, each a disc with a darker rim, two eyes and a mouth: furious (red, horns, angry slanting "
                "eyes, a grin of teeth), unhappy (orange, square eyes, a frown with a lower lip), neutral (yellow, a "
                "straight mouth), happy (green, a smile), delighted (bright green, an open mouth of white teeth)")

if __name__ == "__main__":
    import fel
    fel.build(SPEC)
