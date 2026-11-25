"""P41 hud_panels round 3: build the six round-3 elements into log/roadmap/P41/work3/hud_panels/.

Usage: py -3.12 re/tools/uivec/areas/hud_panels/run3.py [element id ...]
Two-version elements are written as <id>--a / <id>--b (the builders return {"a": scenes, "b": scenes}).
"""
import os
import sys

os.environ["HUDP_WORK"] = "work3"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import run  # noqa: E402,F401   every earlier builder
import common as C  # noqa: E402
import el_bubbles as EB  # noqa: E402
import el_fix3_plate  # noqa: E402,F401
import el_fix3_figs  # noqa: E402,F401
import el_fix3_trophy  # noqa: E402,F401
import el_fix3_faces  # noqa: E402,F401

ROUND3 = ["match-plate", "golfer-trophy-button", "joecool-tab-staff", "tab-golfer-staff", "member-mood-faces",
          "member-mood-faces-extra"]
TWO = {"golfer-trophy-button", "joecool-tab-staff", "tab-golfer-staff", "member-mood-faces", "member-mood-faces-extra"}


def build(el):
    out = EB.BUILD[el](el)
    if el in TWO:
        for v in ("a", "b"):
            C.CELLS[f"{el}--{v}"] = C.CELLS[el]
            print(f"{el}--{v}", C.compare(f"{el}--{v}", out[v], z=Z.get(el)))
    else:
        print(el, C.compare(el, out, z=Z.get(el)))


Z = {"member-mood-faces": None}

if __name__ == "__main__":
    for el in sys.argv[1:] or ROUND3:
        build(el)
