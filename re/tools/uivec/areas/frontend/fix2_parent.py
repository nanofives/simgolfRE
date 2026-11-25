"""P41 fix round: six elements the parent drew (title menu: championship and exit buttons; difficulty: the four faces),
reassigned to this area. The parent's files (re/tools/uivec/glyphs.py, title.py, diff.py and their json) are only READ: the
bubbles, pads, discs and their paint are the parent's measured recipes (title.scene), and only each glyph is replaced by a
corrected one built here, starting from the parent's fitted shapes.

Mariano's notes and what changed:
- title-exit-button "top line on the door is not rect angle": the parent's fit tilted the lintel (746.75, 546.25 -> 758.75,
  547.5) and thinned the frame lines to 0.75 px. Re-read off a dump: the frame is 1 px lines on the pixel grid, the jamb at
  x 745 (rows 546-551 above the arrow, 562-567 below it), the lintel along row 546 to the door, the floor along row 567 on
  both sides of the door. Drawn as axis-aligned boxes, so the corners are square; the door leaf, its slanted foot, the
  keyhole and the arrow re-read from the same dump.
- title-champ-button "person silhouette needs to be clearer, golf stick is not continuous until the end": the golfer is
  redrawn from its parts (head; a torso that leans back from the shoulders to the hips; the hands at the left shoulder
  after the swing; the rear leg bent back to its foot; the front leg straight), and the club is one round-capped line from
  the hands all the way into an oval club head (the parent's 0.45 px butt-capped line broke up and stopped short of the
  head). The cup is the parent's fitted turned profile.
- diff-easy-face "mouth line a little bit thicker": the smile's half circle 3.1 -> 3.8 px, its end bars 2.2 -> 2.8 px.
- diff-medium-face, diff-hard-face "mouth shape on the right is not reaching the end": the mouth curve (the parent's
  qstroke: flat-cut ends square to the curve, which on the steep right end stop below the cap) is now one round-capped line
  through the same control point whose ends sit in the MIDDLE of the end caps (medium: both caps; hard: the slanted cap on
  the left and the flat bar on the right).
- diff-expert-face "tongue shape is not exactly a circle trimmed": re-read off a dump: the light inside of the open mouth is
  a circle (centre 251.75, 527.8, radius 9.25) with its top cut flat at row 521, and the tongue is a 3.4 px stub hanging
  from the middle of that flat top into a round end (radius 2.7). The parent drew a full oval inside with a thin stem and
  a dot.
Usage: py -3.12 fix2_parent.py title | diff
"""
import copy
import json
import os
import sys

import numpy as np

import common as c
import fix2 as F
from common import fit, sdf

UIVEC = c.UIVEC


def exit_glyph():
    frame = F.U(F.box(745.0, 546.0, 746.0, 552.0), F.box(745.0, 546.0, 751.0, 547.0),
                F.box(745.0, 562.0, 746.0, 568.0), F.box(736.0, 567.0, 746.0, 568.0), F.box(755.0, 567.0, 764.0, 568.0))
    door = F.sub(F.poly((750.4, 546.0), (759.0, 546.0), (759.0, 568.0), (755.3, 568.0), (750.4, 561.6)),
                 F.circ(752.5, 554.2, 0.8))
    arrow = F.U(F.box(735.0, 556.0, 742.6, 559.0), F.poly((742.0, 552.9), (750.7, 557.5), (742.0, 562.1)))
    return F.U(frame, door, arrow)


def champ_glyph(fitted):
    cup = fitted["of"][0]
    head = F.circ(521.4, 484.7, 1.45)
    torso = {"t": "poly", "rings": [[[519.6, 486.0], [523.1, 486.0], [525.2, 493.8], [521.2, 494.1]]], "smooth": 1}
    arms = F.path([[[520.9, 486.9], [519.9, 486.8]]], 1.2)          # the hands at the left shoulder
    club = F.path([[[520.0, 486.8], [517.2, 487.3], [514.4, 487.8]]], 0.8)
    club_head = F.circ(513.9, 487.9, 0.95)
    leg_rear = F.path([[[522.5, 493.6], [520.7, 495.4], [518.8, 497.3]]], 1.6)
    foot_rear = F.ell(518.3, 497.7, 1.1, 0.7)
    leg_front = F.path([[[524.3, 493.8], [524.5, 497.7]]], 1.55)
    return F.U(cup, head, torso, arms, club, club_head, leg_rear, foot_rear, leg_front)


def mid(seg):
    (ax, ay), (bx, by) = seg["pts"]
    return [(ax + bx) / 2, (ay + by) / 2]


def easy_glyph(fitted):
    s = copy.deepcopy(fitted)
    s["of"][3]["w"] = 3.8
    s["of"][4]["w"] = 2.8
    s["of"][5]["w"] = 2.8
    return s


def medium_glyph(fitted):
    s = copy.deepcopy(fitted)
    q = s["of"][3]
    a, b = mid(s["of"][4]), mid(s["of"][5])
    s["of"][3] = F.path([F.quad_pts(a, q["p"][1], b, 24)], q["w"][0])
    return s


def hard_glyph(fitted):
    s = copy.deepcopy(fitted)
    q = s["of"][5]
    a = mid(s["of"][6])
    bar = s["of"][7]["pts"]
    b = [q["p"][2][0] - 0.4, (bar[0][1] + bar[1][1]) / 2]        # into the middle of the flat bar
    s["of"][5] = F.path([F.quad_pts(a, q["p"][1], b, 24)], q["w"][0])
    return s


def expert_glyph(fitted):
    s = copy.deepcopy(fitted)
    face = s["of"][0]["a"]                                        # eyes, nose, mouth oval (parent's fit)
    inside = {"t": "inter", "of": [F.circ(251.75, 527.8, 9.25), {"t": "half", "p": [0.0, 521.3], "n": [0.0, -1.0]}]}
    tongue = F.U(F.box(249.6, 520.0, 253.0, 526.6, 0.4), F.circ(251.3, 528.3, 2.7))
    return F.U(F.sub(face, inside), tongue)


def run(screen):
    sys.path.insert(0, UIVEC)
    import title
    if screen == "diff":
        import diff
        diff.setup()
        todo = {"easy": ("diff-easy-face", easy_glyph), "medium": ("diff-medium-face", medium_glyph),
                "hard": ("diff-hard-face", hard_glyph), "expert": ("diff-expert-face", expert_glyph)}
    else:
        todo = {"champ": ("title-champ-button", champ_glyph), "exit": ("title-exit-button", lambda f: exit_glyph())}
    base_idx, base_pal, un_idx, un_pal, mo_idx, mo_pal = (title.SCREEN_LOAD or title.load_art)()
    art_i = title.composite(base_idx, base_pal, un_idx, un_pal).astype(float)
    art_h = title.composite(base_idx, base_pal, mo_idx, mo_pal).astype(float)
    base_rgb = base_pal[base_idx].astype(float)
    meas = title.measure(verbose=False)
    fitted = json.load(open(title.GLYPHS))
    for name, (eid, fn) in todo.items():
        if not F._only(eid):
            continue
        m = meas[name]
        shape = fn(fitted[name]["shape"])
        dx = round(m["hover"]["c"][0] - m["idle"]["c"][0], 2)
        dy = round(m["hover"]["c"][1] - m["idle"]["c"][1], 2)
        m["idle"]["shape"] = shape
        m["hover"]["shape"] = F.mv(shape, dx, dy)
        sc_i = title.scene(meas, False, only=[name])
        sc_h = title.scene(meas, True, only=[name])
        cx, cy, r = m["idle"]["c"]
        pad = 7
        rect = (int(cx - r - pad), int(cy - r - pad), int(2 * r + 2 * pad), int(2 * r + 2 * pad))
        hcx, hcy, _ = m["hover"]["c"]
        rect_h = (int(hcx - r - pad), int(hcy - r - pad), rect[2], rect[3])
        idle_bg = base_rgb if screen != "diff" else None
        st = [{"name": "idle", "art": art_i, "scene": sc_i, "rect": rect}]
        if idle_bg is not None:
            st[0]["bg"] = idle_bg
        st.append({"name": "hover", "art": art_h, "scene": sc_h, "rect": rect_h})
        F.deliver(eid, st, HOW[eid], CHANGED[eid], z=8 if 2 * r + 2 * pad < 120 else 6,
                  extra={"glyph": shape, "hover_shift": [dx, dy], "bubble": m["idle"]["c"]})


HOW = {
    "title-exit-button": "the parent's matte disc with the exit glyph re-read off a dump: a door frame of 1 px lines on the "
                         "pixel grid (jamb at x 745 above and below the arrow, lintel along row 546, floor along row 567 on "
                         "both sides), the door leaf with its slanted foot and a keyhole, an arrow (3 px shaft, head 8.6 "
                         "long and 9.2 tall) pointing into the door; hover the same shapes moved with the disc",
    "title-champ-button": "the parent's glass bubble with the trophy: the parent's fitted turned cup and a golfer from his "
                          "parts (head, leaning torso, hands at the left shoulder, rear leg bent back to "
                          "its foot, front leg straight) holding a club that is one round-capped line from the hands into "
                          "an oval club head; hover the same shapes moved with the bubble",
    "diff-easy-face": "the parent's face with the smile 3.8 px thick (half circle) and its end bars 2.8 px",
    "diff-medium-face": "the parent's face with the smile as one round-capped curve (same control point) whose ends sit in "
                        "the middle of both end caps",
    "diff-hard-face": "the parent's face with the smirk as one round-capped curve (same control point) from the middle of "
                      "the slanted left cap into the middle of the flat right bar",
    "diff-expert-face": "the parent's eyes, nose and leaning mouth oval; the light inside of the mouth a circle with its "
                        "top cut flat at row 521; the tongue a 3.4 px stub from the middle of that flat top into a round "
                        "end (radius 2.7)",
}
CHANGED = {
    "title-exit-button": "frame drawn as axis-aligned 1 px boxes: the lintel is level and meets the jamb at a right angle "
                         "(the fit had tilted it); leaf, keyhole and arrow re-read",
    "title-champ-button": "golfer clearer (torso, arms, bent rear leg with a foot, straight front leg) and the club one "
                          "continuous round-capped line that runs into the club head",
    "diff-easy-face": "mouth line 3.1 -> 3.8 px, bars 2.2 -> 2.8 px",
    "diff-medium-face": "the curve's right (and left) end now reaches into the middle of its cap (round-capped line instead "
                        "of a flat-cut qstroke that stopped short)",
    "diff-hard-face": "the curve now runs up into the middle of the right bar (it stopped under it) and starts in the left "
                      "cap's middle",
    "diff-expert-face": "inside of the mouth is a circle trimmed flat at the top, the tongue a thick stub with a round end "
                        "hanging from that flat top (was a full oval with a thin stem and a dot)",
}


if __name__ == "__main__":
    run(sys.argv[1] if len(sys.argv) > 1 else "title")
