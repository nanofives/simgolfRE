"""Write log/roadmap/P41/work/hud_palettes/results.tsv: one row per element of the area (roster screens In-game HUD, Terrain,
Building, Amenities and Elevation palette). Scores are recomputed from each element's saved scene: interior_mad = the mean over
states of the mean absolute RGB difference on the cut's opaque pixels (kept-art pixels included), edge_px = the summed count of
pixels whose coverage disagrees (opaque in the art but left empty, or drawn where the art is transparent)."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hp  # noqa: E402

SCREENS = {"In-game HUD", "Terrain palette", "Building palette", "Amenities palette", "Elevation palette"}
LINE = "the thin tray line that crosses the cut's lower right (a connector of the fan behind the button) is not drawn"
UNSURE = {
    "hud-zoom-in": LINE, "hud-zoom-out": LINE, "hud-rotate-right": LINE, "hud-rotate-left": LINE, "hud-info": LINE,
    "hud-pause": LINE,
    "hud-tools": LINE + "; wrench read as a shaft with a C-ring jaw at each end (gaps up-left and down-right); check the jaws' size",
    "hud-golfers-mode": "two golfer profiles read from a binary map; faces and the raised hand are approximate (look at the profiles)",
    "hud-course-mode": "golf-hole picture: hills, green, island and fairway edge authored from a binary map; check the fairway's S",
    "hud-buildings-mode": "the ground grid's platform edge drawn as half an ellipse; check against the art's bowl",
    "hud-mode-base": "fan outline is the cut's simplified extent; mode bubbles come out a little larger than the art; the stems under "
                     "the mode bubbles and the thin lines under the small buttons are not drawn",
    "hud-arc-bracket": "band edge colours (light lavender above, navy below) come out muddy on the medium and small sizes",
    "amenities-flowers-tab": "four rose heads with a light curl each, two leaves, three stems; the art's petals are denser than this",
    "amenities-tree-tab": "umbrella canopy and three limbs; the art's trunk is more S-shaped",
    "amenities-rotate": "the small brown mark under the arrow's tail in the art is not drawn",
    "terrain-hole-button": "pennant drawn as one waving shape with the light fold beside the pole; the two light marks in the cup are 1 px",
    "course-cash-medal": "the ball's big white highlight on the 'a' shade is weaker than the art",
    "course-fun-medal": "the laughing face is tilted in the art; mine is upright",
    "course-skill-medal": "'figure with a pin' read as a ring head and a bent line; I am not sure what it depicts",
    "course-fun-box": "medal as on the loose medal (same notes)",
    "course-skill-box": "medal as on the loose medal (same notes)",
    "course-title-badge": "corner curves within 1-2 px of the art (143 edge pixels along them)",
    "gb-mood": "five faces drawn as discs with simple eyes and mouths; the devil's teeth come out grey",
    "gb-theatre-mask": "masks as two ovals with a lit gold half; the art's faces are more detailed",
    "gb-hand-pointer": "read as a grey arch on two legs; I am not sure what it depicts",
    "gb-eye": "almond outline thicker at the corners than the art",
    "gb-crossed-clubs": "club heads drawn blocky as in the art; shafts slightly heavy",
    "gb-trophy": "colours shaded by a measured gradient; the art's highlight pixels are finer",
    "gb-note-page": "read as a notebook seen from the side (page edges as stripes); check what it depicts",
    "building-slot": "pillar faces flat-ish; the art's outline is crisper",
    "building-palette": "slot pillars run down behind the lower row; the recess panel is lighter than the art",
    "amenities-palette": "recess outline approximate (curved left side, rounded right end); thumbnails kept as art",
    "elevation-point": "mound and arrows kept as art (rendered 3D)",
    "elevation-square": "mound and arrows kept as art (rendered 3D)",
    "elevation-area": "mound, grid and arrows kept as art (rendered 3D)",
    "elevation-smooth": "grass disc, ball and trails kept as art (rendered 3D)",
    "amenities-strip": "faint cubes drawn as mid and light iso faces read from a three-level map",
}


def main():
    r = json.load(open(os.path.join(hp.ROSTER, "roster.json")))
    fb = json.load(open(os.path.join(hp.ROSTER, "feedback_2026-11-24.json")))
    rows = []
    for e in r["elements"]:
        if e["screen"] not in SCREENS or e.get("status") == "done":
            continue
        f = fb.get(e["id"], {})
        if f.get("verdict") == "art":
            rows.append([e["id"], "kept_art", "left as art (marked art)", "", "", ""])
            continue
        p = os.path.join(hp.OUT, e["id"] + ".json")
        if not os.path.exists(p):
            rows.append([e["id"], "blocked", "not built", "", "", ""])
            continue
        d = json.load(open(p))
        scenes = {st: v["scene"] for st, v in d["states"].items()}
        sc = hp.score(e["id"], scenes, z=3)
        mad = sum(v[0] for v in sc.values()) / len(sc)
        edge = sum(v[1] for v in sc.values())
        how = d.get("how", "").replace("\t", " ").replace("\n", " ")
        note = UNSURE.get(e["id"], "")
        if f.get("note"):
            note = (note + "; " if note else "") + f"Mariano's note: {f['note']}"
        rows.append([e["id"], "done", how, f"{mad:.2f}", str(edge), note])
        print(f"{e['id']}: {mad:.2f} / {edge}", flush=True)
    hp.write_results(rows)
    print(os.path.join(hp.OUT, "results.tsv"))


if __name__ == "__main__":
    main()
