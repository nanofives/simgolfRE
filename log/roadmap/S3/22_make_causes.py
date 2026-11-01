"""S3.22: 22_rows.tsv (measured by 22_rows.py) + the cause rules below -> 22_causes.tsv.
    ~/bin/py -3.12 log/roadmap/S3/22_make_causes.py"""
import csv
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
rows = list(csv.DictReader(open(HERE / "22_rows.tsv", encoding="utf-8"), delimiter="\t"))

CAUSE = {
    ("cov_b_holes18", "holes"): ("animated scene sprites differ (frame choice)", "scene family: queue entries of the water spray / ripples / hole flags",
                                 "2,815 of 2,829 px are in the original's root at renderSpriteQueue's return (q_before): the layers up to the queue. "
                                 "Components: 820 px water ripples at the left shore, three fountain sprays (416, 381, 359 px), 26-px 5x12 flag poles. Image 22_img/cov_b_holes18.holes.png"),
    ("cov_k3_camera2", "y_inj4"): ("animated scene sprites differ (frame choice)", "scene family: fountain spray sprite",
                                   "411 of 414 px in one box at the fountain; all q_before (the queue root already has the original's spray). Image 22_img/cov_k3_camera2.y_inj4.png"),
    ("cov_k3_camera", "y_inj3"): ("overlay line missing", "an antialiased red line from the `Benches` label diamond to the amenities panel (drawMapLine / Terrain::drawLine path of the hover tip); not found by name",
                                  "847 px in one box 398,275..431,496; 724 q_after: drawn after the sprite queue. The port draws the diamond outline and no connector. Image 22_img/cov_k3_camera.y_inj3.png"),
    ("golfer_stories", "season_end"): ("scene sprite missing (translucent ellipse)", "scene family: an alpha sprite (drawAlpha window) the port does not queue or draw",
                                       "685 px, one box 60,70..102,90, max difference 4 levels; 653 q_before (in the original's queue root), 32 q_after. The original lightens the water under the logo, the port has no ellipse. Image 22_img/golfer_stories.season_end.png"),
    ("cov_r_s2_screens", "hire"): ("body missing (addSprite FLIC cells)", "addSprite of the four employee figures inside showHireEmployeeDialog",
                                   "four boxes 473 / 361 / 314 / 249 px (x 556..586) in the four list rows; 1,376 q_after (drawn after the queue: the modal dialog). Image 22_img/cov_r_s2_screens.hire.png"),
    ("cov_r_s2_screens", "course"): ("terrain edge (map corner)", "terrain pass at the screen-edge diagonal x 781..799, y 451..466",
                                     "65 px (20 beyond one level), all q_before: the original's root at the queue return already has its edge pixels; the port's dark rim runs along the same diagonal but shifted. No terrain recording for this scenario. Image 22_img/cov_r_s2_screens.course.png"),
    ("cov_r_s2_screens", "radial_hover"): ("terrain edge (map corner)", "same box as `course`", "identical components to `course` (65 px, box 781,451..799,466)"),
}
for c in ("rep_comments", "rep_histograph", "rep_financial", "rep_report1", "rep_shortcuts"):
    CAUSE[("cov_r_s2_screens", c)] = ("terrain edge (map corner), dimmed", "same edge as `course`, under the report's dimming pass",
                                      "20 px in box 788,457..799,466 (7 q_before, 13 q_after: the dim band reads the edge pixels); same edge as `course` with the dim applied")
CAUSE[("cov_r_s2_screens", "rep_roster")] = ("terrain edge (map corner), dimmed", "same edge as `course`", "11 px in box 795,462..799,466")
for sc in ("cov_b_inject", "cov_b_inject2"):
    for c in ("zoom_box", "forced_events", "end"):
        CAUSE[(sc, c)] = ("other: 4x4 marker drawn after the queue differs in colour", "unidentified 2D draw after renderSpriteQueue (a 4x4 dot next to a label)",
                          "15 px, one 4x4 box, max difference 31 levels (port black, original pale pink), all q_after; box moves between checkpoints. No terrain pixel involved. Image 22_img/" + sc + "." + c + ".png")


def small(r):
    px = int(r["px"])
    return ("overlay line / ring edge, one level (U-1370 class)", "Terrain::drawLine coverage at a line or ring edge (drawMapLine, selection ring, shot arc)",
            f"{px} px in small boxes ({r['comps'].split(';')[0].split(':', 2)[1]} first), max difference 1-2 levels, along a diagonal or ring; q_before {r['q_before']} / q_after {r['q_after']}")


out = []
for r in rows:
    k = (r["scenario"], r["checkpoint"])
    cause, body, ev = CAUSE.get(k) or small(r)
    box = r["comps"].split(";")[0].split(":")[1] if r["comps"] else ""
    out.append([r["scenario"], r["checkpoint"], r["px"], r["beyond"], cause, body, box, ev])
with open(HERE / "22_causes.tsv", "w", encoding="utf-8", newline="\n") as f:
    f.write("scenario\tcheckpoint\tpx\tbeyond\tcause\tbody_or_call\tbox\tevidence\n")
    for o in out:
        f.write("\t".join(o) + "\n")
from collections import Counter
cnt = Counter(o[4] for o in out)
for k, v in cnt.most_common():
    print(v, k)
