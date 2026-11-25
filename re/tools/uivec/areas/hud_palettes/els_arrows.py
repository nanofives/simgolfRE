"""amenities-page-left / -right: a small lilac tab shaped like an arrow head, holding a lighter triangle pointing the same way
(cream idle, near white on hover, dim when disabled). Read with cmap.py and the cut's runs (2026-11-24); local coordinates."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geom import poly  # noqa: E402

STOPS = [0.0, 0.8, 1.6, 2.6, 4.0]
SPECS = [
    dict(id="amenities-page-left", paint={},
         parts=[("tab: an arrow-head shape pointing left", poly((6, 0), (11, 0), (11, 22), (7, 22), (0, 11.5)), "tab"),
                ("triangle pointing left, flat face measured per state", poly((1.2, 11.5), (9.8, 1.6), (9.8, 21.4)), "arrow")],
         how="a lilac tab shaped like an arrow head pointing left, holding a lighter triangle pointing left (its face cream / "
             "white / dim by state, measured)"),
    dict(id="amenities-page-right", paint={},
         parts=[("tab: an arrow-head shape pointing right", poly((0, 0), (2, 0), (11, 11.5), (2, 23), (0, 23)), "tab"),
                ("triangle pointing right, flat face measured per state", poly((9.8, 11.5), (1.2, 1.6), (1.2, 21.6)), "arrow")],
         how="a lilac tab shaped like an arrow head pointing right, holding a lighter triangle pointing right (its face cream / "
             "white / dim by state, measured)"),
]

if __name__ == "__main__":
    import fel
    for sp in SPECS:
        fel.build(sp)
