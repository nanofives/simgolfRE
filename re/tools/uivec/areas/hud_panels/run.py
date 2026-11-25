"""P41 hud_panels: run every element builder.
Usage: py -3.12 re/tools/uivec/areas/hud_panels/run.py [--paint | --shadows] [element id ...]
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import el_bubbles as EB  # noqa: E402
import el_employee  # noqa: E402,F401
import el_golfer  # noqa: E402,F401
import el_blue  # noqa: E402,F401
import el_marks  # noqa: E402,F401
import el_pads  # noqa: E402,F401
import el_slabs  # noqa: E402,F401
import el_plate  # noqa: E402,F401
import el_trays  # noqa: E402,F401
import el_tan2  # noqa: E402,F401   fix round: tan bubble glyphs re-read
import el_small2  # noqa: E402,F401   fix round: club / crossed / trophy
import el_faces2  # noqa: E402,F401   fix round: mood faces
import el_pads2  # noqa: E402,F401   fix round: shot pads
import el_ped2  # noqa: E402,F401   fix round: pedestal
import el_flat2  # noqa: E402,F401   fix round: bars, slider, page arrows, match plate, story band
import el_figs  # noqa: E402,F401   fix round: figures with volume (overrides el_blue's glyphs)
import el_trays2  # noqa: E402,F401   fix round: trays recomposed

if __name__ == "__main__":
    EB.main(sys.argv[1:])
