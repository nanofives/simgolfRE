"""P41 hud_panels round 7: write log/roadmap/P41/work7/hud_panels/results.tsv (id, status, changed, unsure).
The per-state decisions (moves, shadow choice with its art error) are in _notes/<id>.json, written by fix7.py."""
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fix7 as F  # noqa: E402

YEL = ("a yellow instance keeps every shape and the navy glyph; only its rim and body paint are re-measured on its "
       "yellow art")
TAB_BASE = {
    "tab-golfer-staff": "BASE of the staff family (Mariano's idle figure, round 4): scene unchanged. The family image "
                        "shows all five instances (yellow on the Employee tray, blue on the Member and Action trays, "
                        "the two cuts) built from this one scene",
    "tab-golfers-group": "BASE of the group family (the akimbo trio of Mariano's picture): scene unchanged; the family "
                         "image shows the yellow Member-tray tab, the blue Action / Employee tray tabs and both cuts "
                         "built from it",
    "tab-golfer": "BASE of the golfer family: scene unchanged; the family image shows the yellow Action-tray tab, the "
                  "blue Member / Employee tray tabs and the three cuts built from it",
}
TAB_COPY = {
    "joecool-tab-staff": "rebuilt as tab-golfer-staff moved by whole pixels: its own measured bubble (rim r 14.81, "
                         "body 13.66) replaced by the base's (14.76, 13.61); figure identical",
    "tab-staff-group": "rebuilt as tab-golfers-group moved by whole pixels: rim r 14.79 / body 13.64 -> the base's 14.83 "
                       "/ 13.68; figure identical",
    "employee-tab-golfer": "rebuilt as tab-golfer moved by whole pixels (rim 14.75 / body 13.60 -> 14.74 / 13.59)",
    "tab-golfer-swing": "rebuilt as tab-golfer moved by whole pixels (rim 14.77 / body 13.62 -> 14.74 / 13.59)",
}
TRAYS = {
    "member-panel": "the three baked tabs replaced by the canonical tab scenes: the YELLOW group tab was the round-1 "
                    "glyph on its own larger bubble (rim r 15.2, body 14.05, halo 18.75 against 14.83 / 13.68 / 16.39; "
                    "the art's dark rim is 13.5 px from the cell centre on yellow and blue tabs alike) with a dark "
                    "smudge in its body paint; the blue staff tab was the round-2 man with a knobbed post; " + YEL,
    "golfer-action-panel": "the three baked tabs replaced by the canonical tab scenes: the YELLOW golfer tab was the "
                           "round-1 glyph on a larger bubble (rim 15.35, body 14.2, halo 18.93); the blue staff tab was "
                           "the round-2 man with a knobbed post; " + YEL,
    "employee-panel": "the three baked tabs replaced by the canonical tab scenes: the YELLOW staff tab was the round-1 "
                      "blob figure on a larger bubble (rim 15.18, body 14.03, halo 18.75); the blue group tab was a "
                      "round-2 variant with the middle figure's arms raised over its head; " + YEL,
}
TAB_UNSURE = ("the yellow staff tab: the shirt between the straps takes the yellow body colour (it is the tab's own "
              "colour showing through, as the mid blue is on the blue tab); the spade blade keeps its blue-grey face "
              "and light (the art has no blade, round-4 rule)")
SMALL = {
    "golfer-club-button": "before: the lavender ground and stub moved 1 px down with the bubble in hover / disabled "
                          "(0.3-0.7 px in selected) although the art's ground never moves (column 36 identical in all "
                          "four cells); disabled rim / body 0.11-0.25 px off idle and r 13.20 / 11.80 against 13.04 / "
                          "11.64. Now: ground held; bubble, rim, body and club glyph = idle's moved 1 px down in "
                          "selected / disabled / hover (the art's bubble drops one row: dark rim row 503 -> 504)",
    "golfer-crossed-button": "same as the club button: ground held (it moved with the bubble before); disabled drift "
                             "0.27 px / r +0.16 removed; bubble + glyph = idle's moved 1 px down in the three states",
    "golfer-trophy-button": "same as the club button: ground held; disabled drift 0.25 px / r +0.16 removed; bubble + "
                            "trophy = idle's moved 1 px down. The approved scene's id was the round-3 tag "
                            "golfer-trophy-button--b; the file now says golfer-trophy-button",
    "employee-hire": "rim, body and glyph were already idle's moved 1 px down (the art's drop); hover's shadow radius "
                     "drift 0.05 removed. Now one structure",
    "employee-fire": "as employee-hire: one structure, 1 px drop in every state other than idle (the art's)",
    "employee-resume": "as employee-hire: one structure, 1 px drop in every state other than idle (the art's)",
    "employee-thumbs-up": "disabled: rim / body / glyph 0.11-0.21 px off idle and r +0.16 -> idle's moved 1 px down; "
                          "hover = idle's moved 1 px down",
    "golfer-stats-button": "disabled: drift r +0.08 removed; hover and disabled = idle's moved 1 px down (the art's)",
    "joecool-stats-button": "disabled: drift r +0.37 removed; hover and disabled = idle's moved 1 px down (the art's)",
    "employee-pedestal": "per row (row 1: idle / selected / disabled; row 2: idle 2 / selected 2 / disabled 2), every "
                         "state = its row's idle: selected's lid sat 0.3 px off and its cylinder 0.16 px taller, "
                         "disabled 0.11 px off, selected 2 was drawn 1 px left and 0.6 px narrower although the art "
                         "does not move any pedestal (outline at row 21 / column 32 identical in all six cells)",
    "employee-page-left": "already one structure (no change)",
    "employee-page-right": "already one structure (no change)",
    "member-page-left": "already one structure (no change)",
    "member-page-right": "already one structure (no change)",
}
for _k in ("info", "mood", "swing", "photo", "story", "history", "thought"):
    SMALL[f"golfer-tab-{_k}"] = ("hover = idle moved 1 px UP (the art's: the bubble rises one row on hover), its shadow "
                                 "radius drift (0.55) replaced by idle's shadow; selected = idle in place with the "
                                 "selected paint")
SMALL["golfer-tab-ok"] = ("hover = idle moved 1 px up (the art's), its shadow 1.0 px off -> idle's (fits the art as "
                          "well: 21.4 vs 22.3); disabled = idle in place")
for _k in ("low", "rising", "flat", "high", "straight"):
    SMALL[f"shot-shape-{_k}"] = ("pad body 0.30-0.35 px drift between states removed; DISABLED was drawn at idle's "
                                 "place but its art sits 1 px lower like selected and hover (outline rows 2 -> 3 and "
                                 "38 -> 39, curve 22 -> 23 at the middle column): now pad + curve = idle's moved 1 px "
                                 "down in selected / disabled / hover, the lavender block held")

def shadow_text(el):
    p = os.path.join(F.OUT, "_notes", el + ".json")
    if not os.path.exists(p):
        return ""
    notes = json.load(open(p))
    kept = []
    for st, lines in notes.items():
        for ln in lines:
            if "own shape kept" in ln:
                kept.append(f"{st} {ln.split('(', 2)[-1].rstrip(')')}")
    return "; ".join(kept)


def main():
    nd = os.path.join(F.OUT, "_notes")
    os.makedirs(nd, exist_ok=True)
    for f in os.listdir(F.OUT):
        if f.startswith("_notes_") and f.endswith(".json"):
            shutil.move(os.path.join(F.OUT, f), os.path.join(nd, f[len("_notes_"):]))
    rows = []
    for el, t in TAB_BASE.items():
        rows.append((el, "done", t, TAB_UNSURE if "staff" in el else ""))
    for el, t in TAB_COPY.items():
        rows.append((el, "done", t, TAB_UNSURE if "staff" in el else ""))
    for el, t in TRAYS.items():
        rows.append((el, "done", t, ((TAB_UNSURE + "; ") if el == "employee-panel" else "") +
                     "the tray scenes embed the tabs: re-export them with the cuts"))
    for el in F.A.MULTI:
        sh = shadow_text(el)
        uns = ""
        if sh:
            uns = ("the cast shadow keeps its own shape where the art's shadow differs (a pressed / disabled button "
                   "throws a shorter shadow): " + sh + " (art error, own vs idle's). Bubble, rim and glyph are one "
                   "structure; ask if the shadow must be idle's too")
        rows.append((el, "done", SMALL[el], uns))
    with open(os.path.join(F.OUT, "results.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("id\tstatus\tchanged\tunsure\n")
        for r in rows:
            f.write("\t".join(x.replace("\t", " ").replace("\n", " ") for x in r) + "\n")
    print(len(rows), "rows")


if __name__ == "__main__":
    main()
