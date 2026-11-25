"""P41 popups round 5: the results.tsv rows (work5/popups/results.tsv)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w5  # noqa: E402

BODY = ("body relit on the round-2 recipe (ib2/disc2, the style fitted on all ten buttons): round 1 had one fitted bevel "
        "paint per button (a colour that changes with depth and edge direction, darkening into the rim on every side) and "
        "a rim colour of its own per state (hover 32,40,80 / 40,48,84, idle 48,56,88); now the same layers as the six "
        "siblings: flat shadow-side base, lit colour fading out from a light spot on the upper left, a lit 1 px inner edge "
        "(light upper left, dark lower right), a reflected-light band inside the lower-left rim, one navy rim 44,48,88 in "
        "every state, the same hover shadow on the tab; glyph shapes are the round-1 scene's own (checked: every round-1 "
        "glyph shape is in the new scene unchanged)")
ROWS = [
    ("info-button-chart", BODY + "; glyph light added as the siblings' round-2 glyphs carry it: the orange line in its "
     "measured core f86000 (was flat 248,110,30) with a pale orange band along its upper-left edge (art f8b888 / f8a870, "
     "paler than any mix of core and tan); the navy axes stay flat (their light pixels are the edge mix, as on the "
     "approved flat money sign)", "the pale edge is 0.55 px: at 8x a wider band read as a second line, so it is soft"),
    ("info-button-checklist", BODY + "; glyph light added: the box frame lit from the upper left like the flag's green "
     "(lilac on the upper or left edge of every side, art 9898b8 at the top-left corner fading to 606880 / 485070), the "
     "check's round-1 light line kept", ""),
    ("info-button-notice", BODY + "; glyph light added like the flag's green: a lilac rim along the bubble's top and "
     "upper-left edge (art a0a8b8 / 9898b8 row 7) over a 2 px band of lighter navy, both stopping at the bubble's middle "
     "(its lower half is flat 283058 in the art); a lighter navy 1 px line on the tail's left edge (707880 / 888098); "
     "the '!' bar and dot sunk into the bubble: a shadow band on their upper-left walls (art 887868 / 585858 on the "
     "left column)", "the art's lilac rim is softer and more graded than this crisp band; drawn crisp to match the flag "
     "sibling's approved rim"),
    ("info-button-pair", BODY + "; no glyph light added: the golfers are 1 px navy lines whose lighter pixels are edge "
     "mix, as on the approved flat money sign", "MAD against the art rises (idle 7.95 -> 9.43) by the same amount as the "
     "siblings' did with this body (money 7.11 -> 9.61), true of all four"),
]
for r in ROWS:
    w5.record(r[0], "done", r[1], r[2])
print(open(w5.c.RESULTS, encoding="utf-8").read())
