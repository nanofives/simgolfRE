"""uivec: measure the style again on the saved geometry (linear fits only), save it and run the acceptance pass.
Usage: py -3.12 re/tools/uivec/remeasure.py
"""
import copy
import json
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import accept   # noqa: E402
import recipes  # noqa: E402


def main():
    b = accept.Bench()
    style = copy.deepcopy(recipes.STYLE)
    for hover in (False, True):
        accept.measure_bubble_style(b, style, hover, use_spot=True)
        accept.measure_pad_style(b, style, hover)
        accept.measure_disc_style(b, style, hover)
    with open(os.path.join(os.path.dirname(__file__), "style.json"), "w") as f:
        json.dump(style, f)
    recipes.STYLE.update(style)
    accept.main()


if __name__ == "__main__":
    main()
