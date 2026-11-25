"""P41 fix round: the 'unsure' notes of this round's results.tsv rows (what Mariano should look at), written after the runs."""
import fix2 as F

UNSURE = {
    "editor-outfit": "drawn to read as a garment (collar points, sleeves set off by seams, cinched waist, hems); the seams are "
                     "the art's light pixels, the rounding of the outline is my choice for a cloth look",
    "editor-body": "read off a 30 px figure: the right leg is one thin line and the right arm a thin outlined tube because "
                   "that is all the art shows of them",
    "editor-hair": "the light gap round the face stops at the temples (the fringe joins the face there), as the dump shows",
    "editor-save": "the art's light lines are mid-tone (antialiased) at 1x; drawn as crisp 1 px cuts",
    "world-save": "the art's light lines are mid-tone at 1x; drawn as crisp 1 px cuts; the socket ring is the first round's",
    "editor-pill": "the tail cells' dark swoosh under the pill is the backdrop's curve (background), not a shadow",
    "editor-pill-tall": "the selected cells have no shadow in the art, so none is drawn there",
    "editor-page": "the bubble shadow is one recipe (two soft dark copies of the circle) fitted on the five page bubbles "
                   "and used for every bubble on the editor screen",
    "editor-backdrop": "the cream backing behind the pills, the portrait window, the wires and the reset / OK socket rings "
                       "stay texture; the family / head / gender glyphs are the approved first-round ones in the new paint",
    "load-info-bar": "the bar's inside is flat navy; the art lets a faint trace of the frame's ball show through it",
    "pro-bottom-bar": "the background seen through the veil is the art's own backdrop recovered by undoing the veil; the "
                      "veil itself is the first round's fit",
    "world-backdrop": "the 16 capsules each have their own fitted edge circle (radius about 51); a 3x close-up of two "
                      "capsules is in world-backdrop--capsules-closeup.png",
    "title-champ-button": "the golfer is read off a figure about 15 px tall; the club's shaft is barely visible in the art "
                          "(mid-tone), drawn 0.8 px so it reads as continuous",
    "diff-expert-face": "eyes, nose and the mouth oval are the parent's fitted shapes; only the inside and the tongue changed",
}


def main():
    rows = F._rows()
    for eid, u in UNSURE.items():
        if eid in rows:
            r = rows[eid]
            F.row(eid, r["status"], r["how"], r["changed"], u)


if __name__ == "__main__":
    main()
