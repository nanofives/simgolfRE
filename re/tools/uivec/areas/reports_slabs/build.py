"""Build one report sheet from its pieces, fit the paints on the art, write the comparison images, scenes and rows.

Usage: py -3.12 build.py <key>       keys: sheets.SHEETS
Writes log/roadmap/P41/work/reports_slabs/<element id>.png / .json, and geom/<key>_scene.json (the whole sheet).

Pieces (parts.py): slab (rpoly minus its sunken windows), bands (the windows' cream), wires (navy strokes), plates and
pills (a navy outer shape with the bevel-painted inner shape on it). Paints are fitted on the art (fit_paints): the slab,
band and inner-surface bevels by least squares, the line widths of each pill kind by a small grid, the ink shadow by a grid.
"""
import json
import os
import sys

import numpy as np

import common as C
import slabkit as K
import parts as P
import sheets
import autoread
import boxes

ROSTER = os.path.join(C.ROOT, "log", "roadmap", "P41", "roster")
sys.path.insert(0, ROSTER)

LILAC = (191, 193, 251)
CREAM = (255, 247, 231)
STOPS = {
    "slab": [0, 1, 2, 3, 4.5, 6.5, 9, 12, 16, 21, 28, 40],
    "pill": [0, 0.5, 1, 1.5, 2, 3, 4, 6],
    "plate": [0, 0.5, 1, 1.5, 2, 3, 4, 6, 9, 13],
    "band": [0, 1, 2, 3, 4, 6, 9, 14, 22],
    "well": [0, 1, 2, 3, 4.5, 6.5, 9, 12, 16, 22],
}
DIR_MAX = {"pill": 4.5, "plate": 5.0, "band": 6.0, "slab": 10.0, "well": 10.0}
W_GRID = {"pill": [{"t": t, "b": b, "s": s} for t in (1.0, 1.3, 1.6) for b in (1.3, 1.6, 2.0) for s in (1.6, 2.0, 2.4)],
          "plate": [{"t": t, "b": b, "s": s} for t in (1.0, 1.3, 1.6) for b in (2.2, 2.6, 3.0) for s in (2.2, 2.6, 3.0)]}
ORDER = {"cream_row": 2.5, "slab": -1, "well": -0.5, "band": 0, "hair": 0.3, "cell": 0.5, "dot": 3.5, "ok": 4, "wire": 1, "plate": 2, "pill_lilac": 3, "pill_cream": 3, "pill_other": 3, "pill_ring": 3}


def roster_crops(sheet_name):
    """{element id: [(state, (x, y, w, h)), ...]} in sheet pixels, trimmed as the roster trims them."""
    import build_roster as R
    R.ELEMENTS.clear()
    R.reports()
    out = {}
    idx, _ = C.fit.load_sheet(C.IS + sheet_name)
    mask = idx < 0xfe
    for e in R.ELEMENTS:
        if os.path.basename(e["sheet"]) != sheet_name:
            continue
        rows = []
        for spec in e["crops"]:
            st, x, y, w, h = spec[:5]
            opts = spec[5] if len(spec) > 5 else {}
            x0, y0, x1, y1 = max(0, x), max(0, y), min(800, x + w), min(600, y + h)
            if opts.get("trim", True):
                m = mask[y0:y1, x0:x1]
                ys, xs = np.nonzero(m)
                x0, y0, x1, y1 = x0 + xs.min(), y0 + ys.min(), x0 + xs.max() + 1, y0 + ys.max() + 1
            rows.append((st, (int(x0), int(y0), int(x1 - x0), int(y1 - y0))))
        out[e["id"]] = {"rows": rows, "what": e["what"], "kind": e["kind"]}
    return out


def grid_of(S, pad, shape=(600, 800)):
    x0, y0, x1, y1 = K.bbox(S, pad)
    i0, j0 = max(int(np.floor(x0)), 0), max(int(np.floor(y0)), 0)
    i1, j1 = min(int(np.ceil(x1)), shape[1]), min(int(np.ceil(y1)), shape[0])
    yy, xx = np.mgrid[j0:j1, i0:i1]
    return (j0, j1, i0, i1), xx + 0.5, yy + 0.5


class Sheet:
    def __init__(self, key):
        self.key = key
        self.cfg = sheets.SHEETS[key]
        self.name = self.cfg["sheet"]
        self.art, self.alpha, self.raw = C.art(self.name)
        self.meas = self.raw.copy()          # for line reading: outside the slab (and the guide marks) counts as white
        self.meas[self.alpha < 0.5] = 255.0
        gp = os.path.join(C.HERE, "geom", key + ".json")
        self.geom = json.load(open(gp)) if os.path.exists(gp) else {}
        self.valid = np.zeros(self.alpha.shape, bool)
        for y0, y1 in self.cfg["rows"]:
            self.valid[y0:y1] = True
        self.full = (self.alpha > 0.995) & self.valid
        self.slab_valid = self.valid.copy()
        if self.cfg.get("slab_rows"):
            self.slab_valid[:] = False
            for y0, y1 in self.cfg["slab_rows"] + [r for e in self.cfg.get("extra_slabs", []) for r in e["rows"]]:
                self.slab_valid[y0:y1] = True
        self.pieces = []
        self.widths = {}
        self.W = {g: {"t": 1.6, "b": 2.0, "s": 2.0} for g in ("pill_lilac", "pill_cream", "plate", "pill_ring", "pill_other")}
        self.paints = {}
        self.shadow = None

    # ------------------------------------------------------------------ pieces
    def add(self, name, group, **kw):
        d = {"name": name, "group": group}
        d.update(kw)
        self.pieces.append(d)
        return d

    def gather(self):
        cfg = self.cfg
        outline = self.geom.get("outlines", {}).get("slab", cfg["outlines"]["slab"])
        slab = {"t": "reports_slabs.rpoly", "verts": outline}
        holes = [{"t": "reports_slabs.rpoly", "verts": h} for h in cfg.get("holes", [])]
        if holes:
            slab = {"t": "sub", "a": slab, "b": holes[0] if len(holes) == 1 else {"t": "union", "of": holes}}
        slab_piece = self.add("slab", "slab", shape=slab)
        # a second outline for a piece whose foot sits elsewhere (Tournament's first end band)
        for i, e in enumerate(cfg.get("extra_slabs", [])):
            self.add(f"slab_x{i}", "slab", shape={"t": "reports_slabs.rpoly", "verts": e["verts"]}, rows=e["rows"])
        for i, h in enumerate(holes):
            self.add(f"band{i}", "band", shape=h, fill=list(cfg.get("band_fill", CREAM)))
        for i, fb in enumerate(cfg.get("plates", [])):
            self.add(f"plate{i}", "plate", meas=P.plate_measure(self.meas, fb), fill_box=fb)
        skip = [tuple(b) for b in cfg.get("not_pills", [])] + [tuple(b) for b in self.ok_boxes()]
        found = []
        pill_cols = [(LILAC, "pill_lilac"), (CREAM, "pill_cream")] + [(tuple(c), g) for c, g in cfg.get("pill_colours", [])]
        self.pill_fills = []
        self.box_r = {}
        for col, grp in pill_cols:
            for (x0, y0, x1, y1, n, comp) in P.fill_components(self.raw, self.alpha, col):
                if any(x0 >= a and y0 >= b and x1 <= c and y1 <= d for a, b, c, d in skip):
                    continue
                if (x1 - x0) < 6 or (y1 - y0) < 4:
                    continue
                # a pill's flat fill has round ends; a fill with square corners is a table cell (autoread.cells)
                _, _, _, _, rr, iou = boxes.fit_rrect(comp)
                box_r = None
                if rr < 0.3 * min(x1 - x0, y1 - y0):
                    sides = P.ringed(self.meas, (x0, y0, x1, y1))
                    cut = (y0 >= 2 and not self.valid[y0 - 2].any()) or (y1 + 1 < 600 and not self.valid[y1 + 1].any())
                    boxed = len(sides) == 4 or (len(sides) == 3 and cut and ("t" not in sides or "b" not in sides))
                    if iou < 0.9 or not boxed:
                        continue          # a table cell (autoread.cells)
                    box_r = rr
                cen, kind = P.measure_pill(self.meas, (x0, y0, x1, y1), square=box_r is not None)
                if cen is None:
                    print("  no line round fill", col, (x0, y0, x1, y1))
                    continue
                self.widths.setdefault(grp, []).append(dict(P.measure_pill.last_widths))
                self.pill_fills.append((x0, y0, x1, y1))
                found.append((grp, kind, cen, (x0, y0, x1, y1)))
                if kind == "box":
                    self.box_r[(x0, y0, x1, y1)] = box_r
        # round 2: pills read by hand where a row piece cuts them (the fill reader cannot see their round ends)
        for grp, kind, cen in cfg.get("pills_manual", []):
            fb = (int(cen[0] + 3), int(cen[1] + 3), int(cen[2] - 3), int(cen[3] - 3))
            found.append((grp, kind, [float(v) for v in cen], fb))
        if cfg.get("ellipse_as_capsule"):
            found = [(g, "capsule" if k == "ellipse" else k, c, b) for g, k, c, b in found]
        found.sort(key=lambda f: (f[2][1], f[2][0]))
        # instances: pills of one kind in one row share their height (and ovals their width): the median, about each
        # instance's own measured centre (one oval of Hole Stats read 2 px taller from a stray dark pixel)
        rows = {}
        for f in found:
            cy = (f[2][1] + f[2][3]) / 2
            key = (f[0], f[1], round(cy / 3))
            rows.setdefault(key, []).append(f)
        for key, fs in rows.items():
            if len(fs) < 3:
                continue
            hh = float(np.median([f[2][3] - f[2][1] for f in fs]))
            cym = float(np.median([(f[2][3] + f[2][1]) / 2 for f in fs]))
            ww = float(np.median([f[2][2] - f[2][0] for f in fs])) if key[1] == "ellipse" else None
            for f in fs:
                c = f[2]
                cx, cy = (c[0] + c[2]) / 2, cym
                c[1], c[3] = cy - hh / 2, cy + hh / 2
                if ww is not None:
                    c[0], c[2] = cx - ww / 2, cx + ww / 2
        # big sunken cream rows read by hand (Low Score / Player Comments): ONE rounded box through the head, the line
        # band and the foot; the row pieces cut it, so the fill reader saw two square boxes and a cell
        crs = cfg.get("cream_rows", [])
        def in_cr(b):
            return any(b[0] >= c["c"][0] - 3 and b[1] >= c["c"][1] - 3 and b[2] <= c["c"][2] + 3 and b[3] <= c["c"][3] + 3
                       for c in crs)
        found = [f for f in found if not (f[0] == "pill_cream" and in_cr(f[3]))]
        for i, c in enumerate(crs):
            x0, y0, x1, y1 = c["c"]
            S = {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2 + c["w"] / 2, (y1 - y0) / 2 + c["w"] / 2],
                 "r": c["r"] + c["w"] / 2, "rot": 0.0}
            I = {"t": "rrect", "c": S["c"], "half": [S["half"][0] - c["w"], S["half"][1] - c["w"]], "r": c["r"] - c["w"] / 2,
                 "rot": 0.0}
            self.add(f"cream_row{i}", "cream_row", shape=S, inner=I, spec=c)
        self.pill_fills += [list(map(int, c["c"])) for c in crs]
        for k, (grp, kind, cen, fb) in enumerate(found):
            self.add(f"{grp}{k}", grp, cen=[round(v, 3) for v in cen], kind=kind, fill_box=fb, box_r=self.box_r.get(fb))
        # table cells, colour bands, legend squares: flat rounded rectangles read on the grid
        well_holes = {}
        # graph paper: a cream window in the slab with 1 px hairlines at the measured spacing (histograph.pcx: verticals
        # every 32 px from x 106, every fifth darker; horizontals every 50 px from y 72)
        pp = cfg.get("paper")
        if pp:
            x0, y0, x1, y1 = pp["box"]
            S = {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": 0.0, "rot": 0.0}
            self.add("paper", "band", shape=S, fill=list(pp["fill"]))
            holes.append(S)
            a, st, b, every, dark, light = pp["v"]
            for k, x in enumerate(range(a, b + 1, st)):
                self.add(f"hair_v{k}", "hair", shape={"t": "seg", "a": [x + 0.5, y0], "b": [x + 0.5, y1], "w": 1.0},
                         fill=list(dark if k % every == 0 else light))
            a, st, b, col = pp["h"]
            for k, y in enumerate(range(a, b + 1, st)):
                self.add(f"hair_h{k}", "hair", shape={"t": "seg", "a": [x0, y + 0.5], "b": [x1, y + 0.5], "w": 1.0},
                         fill=list(col))
        # legend dots: a flat colour disc in a navy ring; a two-colour dot is split at its centre line
        for k, d in enumerate(cfg.get("dots", [])):
            cx, cy, r = d[:3]
            cols = d[3]
            db = cfg.get("dot_box")
            if db:
                # round 2: the dot sits in a rounded-square ring (Histograph: 16 x 16, corners r 5.5, 2 px ring 97 103 146,
                # read on histograph.pcx 102..118 x 539..555), joined to its ring pill by a 2 px stub on the dot's centre row
                box = {"t": "rrect", "c": [cx, cy], "half": [db["half"], db["half"]], "r": db["r"], "rot": 0.0}
                self.add(f"dot{k}", "dot", shape=box, cols=cols, r=r, ring=db)
                pr = [q for q in self.pieces if q["group"] == "pill_ring" and q["cen"][0] > cx and q["cen"][1] < cy < q["cen"][3]]
                if pr:
                    q = min(pr, key=lambda q: q["cen"][0])
                    self.add(f"dot_stub{k}", "hair", shape={"t": "seg", "a": [cx + db["half"] - 0.5, cy],
                                                           "b": [q["cen"][0] + 0.5, cy], "w": db["w"]}, fill=list(db["col"]))
            else:
                self.add(f"dot{k}", "dot", shape={"t": "circle", "c": [cx, cy], "r": r + 1.4}, cols=cols, r=r)
        if cfg.get("auto_cells"):
            skip_c = [tuple(b) for b in cfg.get("not_pills", [])] + [tuple(b) for b in cfg.get("cell_skip", [])] + [
                tuple(b) for b in self.ok_boxes()] + [(c["c"][0] - 3, c["c"][1] - 3, c["c"][2] + 3, c["c"][3] + 3)
                                                      for c in cfg.get("cream_rows", [])] + [
                tuple(f) for f in self.pill_fills]
            inc = tuple(tuple(c) for c in cfg.get("cell_cols", []))
            for k, (x0, y0, x1, y1, r, col, iou) in enumerate(autoread.cells(self.raw, self.alpha, skip_c, include_cols=inc)):
                if not self.valid[y0:y1].any():
                    continue
                S = {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r,
                     "rot": 0.0}
                # only the pale cells (cream, tan, white) are windows into the slab; coloured squares sit on top of it
                on_slab = cfg.get("cells_as_holes") and self.slab_valid[y0:y1].all() and (max(col) - min(col)) < 70                     and tuple(col) not in [tuple(c) for c in cfg.get("cells_on_top", [])]
                well = next((i for i, w in enumerate(cfg.get("wells", [])) if x0 >= w[0] and y0 >= w[1] and x1 <= w[2]
                             and y1 <= w[3]), None)
                if on_slab:
                    # a sunken window: cut out of the slab, or of the well it sits in (whose bevel then shades the
                    # pillars between cells)
                    self.add(f"band_c{k}", "band", shape=S, fill=col, fit=iou)
                    (holes if well is None else well_holes.setdefault(well, [])).append(S)
                else:
                    self.add(f"cell{k}", "cell", shape=S, fill=col, fit=iou)
        # wells: sunken lilac areas cut out of the slab, themselves cut by their cells; painted by their own bevel
        for i, w in enumerate(cfg.get("wells", [])):
            x0, y0, x1, y1, r = w
            W_ = {"t": "rrect", "c": [(x0 + x1) / 2, (y0 + y1) / 2], "half": [(x1 - x0) / 2, (y1 - y0) / 2], "r": r, "rot": 0.0}
            hs = well_holes.get(i, [])
            S = W_ if not hs else {"t": "sub", "a": W_, "b": hs[0] if len(hs) == 1 else {"t": "union", "of": hs}}
            self.add(f"well{i}", "well", shape=S)
            holes.append(W_)
        if holes:
            slab_piece["shape"] = {"t": "sub", "a": {"t": "reports_slabs.rpoly", "verts": outline},
                                   "b": holes[0] if len(holes) == 1 else {"t": "union", "of": holes}}
        if cfg.get("auto_lines"):
            block = np.zeros(self.alpha.shape, bool)
            for p in self.pieces:
                if p["group"] in ("slab", "band", "cell", "well"):
                    continue
                o = self.shapes(p)[0]
                (j0, j1, i0, i1), x, y = grid_of(o, 2)
                block[j0:j1, i0:i1] |= C.sdf.sd(o, x, y) < 0.7
            for b in cfg.get("line_skip", []):
                block[b[1]:b[3], b[0]:b[2]] = True
            yy, xx = np.mgrid[0:600, 0:800]
            for f in cfg.get("ok_feet", []):
                block |= np.hypot(xx + 0.5 - f["c"][0], yy + 0.5 - f["c"][1]) < f["r"] + 1.6
            found = autoread.lines(self.meas, self.alpha, block, self.slab_valid)
            for k, (pts, rr, w) in enumerate(autoread.to_strokes(found)):
                pts = self.into_socket(pts)
                S = P.wire(pts, rr if rr else 0.0, w=round(w, 2), cap="butt")
                self.add(f"line{k}", "wire", shape=S)
        for i, w in enumerate(cfg.get("wires", [])):
            if w[0] == "path":
                S = P.wire(w[1], w[2] if len(w) > 2 else 0.0)
            elif w[0] == "bracket":
                S = P.bracket(*w[1:])
            else:
                raise ValueError(w)
            self.add(f"wire{i}", "wire", shape=S)
        self.pieces.sort(key=lambda p: ORDER.get(p["group"], 5))
        # line widths READ on the art (darkness profile across each side, median over the kind's pieces)
        for g, ws in self.widths.items():
            self.W[g] = {"t": round(float(np.median([w["t"] for w in ws])), 2),
                         "b": round(float(np.median([w["b"] for w in ws])), 2),
                         "s": round(float(np.median([(w["l"] + w["r"]) / 2 for w in ws])), 2)}
        pl = [p for p in self.pieces if p["group"] == "plate"]
        if pl:
            self.W["plate"] = {k: round(float(np.median([p["meas"]["widths"][k] for p in pl])), 2) for k in "tbs"}

    def ok_boxes(self):
        return [[f["c"][0] - f["r"] - 3, f["c"][1] - f["r"] - 3, f["c"][0] + f["r"] + 3, f["c"][1] + f["r"] + 3]
                for f in self.cfg.get("ok_feet", [])]

    def into_socket(self, pts):
        """A wire that stops at a foot bubble's socket (the line reader is blocked there) is run on along its own
        direction until it is under the socket ring, so it reaches the button as the art's does."""
        pts = [list(map(float, p)) for p in pts]
        for f in self.cfg.get("ok_feet", []):
            c = np.array(f["c"], float)
            R = f["r"] - 1.0
            for end, nxt in ((0, 1), (-1, -2)):
                p, q = np.array(pts[end]), np.array(pts[nxt])
                if np.hypot(*(p - c)) > f["r"] + 4.0:
                    continue
                u = (p - q) / max(np.hypot(*(p - q)), 1e-6)
                # |p + t u - c| = R, the root ahead of p
                b = float(np.dot(u, p - c))
                disc = b * b - (float(np.dot(p - c, p - c)) - R * R)
                if disc <= 0:
                    continue
                t = -b - np.sqrt(disc)
                if t > 0:
                    pts[end] = list(map(float, p + t * u))
        return pts

    def extra_items(self):
        """Hand-authored items a sheet adds over its pieces (cfg "extras": "module.function", called with the sheet)."""
        if not hasattr(self, "_extra"):
            self._extra = []
            for ref in self.cfg.get("extras", []):
                mod, fn = ref.split(".")
                self._extra += getattr(__import__(mod), fn)(self)
        return self._extra

    def ok_items(self):
        """The yellow OK bubbles drawn into the slab's foot: ok.py's recipe (approved as report-ok / endofyear-ok-hover),
        measured on this sheet's own foot bubble, without the button's square or stubs."""
        if not hasattr(self, "_ok"):
            import ok
            self._ok = []
            for i, f in enumerate(self.cfg.get("ok_feet", [])):
                its = ok.build(sheet=self.name, cells={"idle": tuple(f["origin"])}, out_id=None, files=False, bare=True)
                for it in its:
                    it["part"] = f"ok{i}"
                self._ok += its
        return self._ok

    def shapes(self, p, W=None):
        """(outer, inner or None) of a piece."""
        g = p["group"]
        if g.startswith("pill"):
            return P.pill_shapes(p["cen"], p["kind"], W or self.W[g], p.get("box_r"))
        if g == "plate":
            return P.plate_shapes(p["meas"], W or self.W[g], *self.cfg.get("plate_r", (6.0, 2.0)))
        return p["shape"], None

    # ------------------------------------------------------------------ paint fitting
    def owner(self):
        own = np.full(self.alpha.shape, -1, int)
        for k, p in enumerate(self.pieces):
            (j0, j1, i0, i1), x, y = grid_of(self.shapes(p)[0], 1)
            d = C.sdf.sd(self.shapes(p)[0], x, y)
            own[j0:j1, i0:i1][d < 0] = k
        er = own.copy()
        es = own.copy()        # eroded only against other pieces (a slab keeps its rim against the outside)
        for dj in (-1, 0, 1):
            for di in (-1, 0, 1):
                sh = np.roll(np.roll(own, dj, 0), di, 1)
                er = np.where(sh == own, er, -2)
                es = np.where((sh == own) | (sh == -1), es, -2)
        self._er_slab = es
        return own, er

    def ink_dist(self):
        D = np.full(self.alpha.shape, 99.0)
        for p in self.pieces:
            if p["group"] in ("slab", "band", "well"):
                continue
            S = self.shapes(p)[0]
            (j0, j1, i0, i1), x, y = grid_of(S, 6)
            D[j0:j1, i0:i1] = np.minimum(D[j0:j1, i0:i1], C.sdf.sd(S, x, y))
        return D

    def fit_surface(self, g, er, ks, W=None, inner=True):
        """Least-squares bevel for group g over its pieces ks (their inner shapes, or the whole shape)."""
        st = STOPS["pill" if g.startswith("pill") else g]
        shapes = [self.shapes(self.pieces[k], W)[1 if inner else 0] for k in ks]
        er_slab = self._er_slab if g in ("slab", "band", "well") else None
        ink = self._ink if g in ("slab", "band", "well") else None

        def sel(i, x, y, d):
            j = (y - 0.5).astype(int)
            ii = (x - 0.5).astype(int)
            src = er_slab if g in ("slab", "band", "well") else er
            m = (src[j, ii] == ks[i]) & self.full[j, ii] & (d < (-0.1 if inner else (-0.3 if g in ("slab", "band") else -0.6)))
            if ink is not None:
                m &= (ink[j, ii] > 3.5) & self.slab_valid[j, ii]
                if g == "slab":       # the slab's paint is learned on its own lilac only (never on an unread cell)
                    a_ = self.art[j, ii]
                    m &= (a_[..., 2] > a_[..., 0] + 20) & (a_[..., 2] > a_[..., 1] + 15)
            return m
        coef, rms = K.fit_bevel(shapes, self.art, sel, st, dir_max=DIR_MAX.get("pill" if g.startswith("pill") else g))
        return K.bevel(1.0, st, coef), rms

    def piece_item(self, p, W=None, paint=None):
        g = p["group"]
        if g == "cream_row":
            sp = p["spec"]
            cream = np.array(sp["fill"], float)
            # sunken: a thin inner shade, deepest under the top line (3 px), 2 px on the sides, 1 px over the bottom
            # (read on PlayComt.pcx x 400 / y 232: top 196 219 243, sides 219 243, bottom 243, then 255 cream)
            prof = {"side": [209, 233.4, 250, 255, 255], "top": [191.4, 214.4, 238.2, 252.6, 255],
                    "bot": [233, 245.4, 255, 255, 255]}
            coef = []
            for k in range(5):
                sd_, tp, bt = prof["side"][k], prof["top"][k], prof["bot"][k]
                c0 = (sd_ + (tp + bt) / 2) / 2
                c3 = sd_ - c0
                c2 = (bt - tp) / 2
                coef.append([list(cream * c0 / 255), [0.0, 0.0, 0.0], list(cream * c2 / 255), list(cream * c3 / 255), [0.0, 0.0, 0.0]])
            fill = {"bevel": {"L": 1.0, "stops": [0, 1, 2, 3, 4], "coef": coef}}
            return K.item([{"shape": p["shape"], "fill": list(sp["line"])}, {"shape": p["inner"], "fill": fill}]) | {"part": p["name"]}
        if g in ("cell", "band", "hair"):
            return K.item([{"shape": p["shape"], "fill": p["fill"]}]) | {"part": p["name"]}
        if g == "dot":
            c, r = p["shape"]["c"], p["r"]
            if p.get("ring"):
                db = p["ring"]
                L = [{"shape": {"t": "outline", "of": {"t": "rrect", "c": c, "half": [db["half"] - db["w"] / 2] * 2,
                                                       "r": db["r"] - db["w"] / 2, "rot": 0.0}, "w": db["w"]},
                      "fill": list(db["col"])}]
            else:
                L = [{"shape": p["shape"], "fill": list(P.NAVY)}]
            inner = {"t": "circle", "c": c, "r": r}
            if isinstance(p["cols"][0], (list, tuple)):
                L.append({"shape": inner, "fill": list(p["cols"][1])})
                L.append({"shape": {"t": "inter", "of": [inner, {"t": "half", "p": c, "n": [0.0, 1.0]}]},
                          "fill": list(p["cols"][0])})
            else:
                L.append({"shape": inner, "fill": list(p["cols"])})
            return K.item(L) | {"part": p["name"]}
        if g == "well":
            L = {"shape": p["shape"], "fill": self.paints["well"], "key": p["name"]}
            if self.cfg.get("slab_rows"):
                L["clip"] = self.slab_clip()
            return K.item([L]) | {"part": p["name"]}
        outer, inner = self.shapes(p, W)
        pt = paint or self.paints.get(g)
        if inner is not None:
            return K.item([{"shape": outer, "fill": pt["line"]}, {"shape": inner, "fill": pt["fill"]}]) | {"part": p["name"]}
        return K.item([{"shape": outer, "fill": pt}]) | {"part": p["name"]}

    def fit_inked(self, g, ks, er):
        """Line widths (grid) + line colour + inner bevel for a pill / plate group: the widths whose render matches the
        art best inside the pieces."""
        best = None
        for W in [self.W[g]]:
            fill, rms = self.fit_surface(g, er, ks, W)
            # the line colour: art pixels in the middle of the line
            cols = []
            for k in ks:
                o, i = self.shapes(self.pieces[k], W)
                (j0, j1, i0, i1), x, y = grid_of(o, 1)
                do, di = C.sdf.sd(o, x, y), C.sdf.sd(i, x, y)
                m = (do < -0.45) & (di > 0.45) & (er[j0:j1, i0:i1] == k) & self.full[j0:j1, i0:i1]
                cols.append(self.art[j0:j1, i0:i1][m])
            c = np.concatenate(cols)
            line = [round(float(v), 1) for v in np.median(c, axis=0)] if len(c) > 10 else list(P.NAVY)
            paint = {"line": line, "fill": fill}
            e, n = 0.0, 0
            for k in ks[:12]:
                it = self.piece_item(self.pieces[k], W, paint)
                b = [int(np.floor(it["box"][0])), int(np.floor(it["box"][1])), int(np.ceil(it["box"][2])), int(np.ceil(it["box"][3]))]
                b = [max(b[0], 0), max(b[1], 0), min(b[2], 800), min(b[3], 600)]
                img = np.kron(self.art[b[1]:b[3], b[0]:b[2]], np.ones((4, 4, 1)))
                C.sdf.render([it], img, 4, origin=(b[0], b[1]))
                low = K.down(img, 4)
                o = self.shapes(self.pieces[k], W)[0]
                (j0, j1, i0, i1), x, y = grid_of(o, 0, )
                m = np.zeros(low.shape[:2], bool)
                do = C.sdf.sd(o, (np.mgrid[b[1]:b[3], b[0]:b[2]][1] + 0.5), (np.mgrid[b[1]:b[3], b[0]:b[2]][0] + 0.5))
                own = er[b[1]:b[3], b[0]:b[2]]
                m = (do < 0.5) & ((own == k) | (do > -0.5)) & self.full[b[1]:b[3], b[0]:b[2]]
                e += float(np.abs(low - self.art[b[1]:b[3], b[0]:b[2]])[m].sum())
                n += int(m.sum())
            score = e / max(n, 1)
            if best is None or score < best[0]:
                best = (score, W, paint, rms)
        print(f"  {g}: {len(ks)} pieces, widths {best[1]}, line {best[2]['line']}, mad {best[0]:.2f}, inner rms {best[3]:.2f}",
              flush=True)
        self.W[g] = best[1]
        self.paints[g] = best[2]
        self.fit_mad = getattr(self, "fit_mad", {})
        self.fit_mad[g] = best[0]

    def fit_paints(self):
        own, er = self.owner()
        groups = {}
        for k, p in enumerate(self.pieces):
            groups.setdefault(p["group"], []).append(k)
        for g in ("plate", "pill_lilac", "pill_cream", "pill_ring", "pill_other"):
            if g in groups:
                self.fit_inked(g, groups[g], er)
        own, er = self.owner()
        self._ink = self.ink_dist()
        for g in ("slab", "well"):
            if g in groups:
                self.paints[g], rms = self.fit_surface(g, er, groups[g], inner=False)
                print(f"  {g}: rms {rms:.2f}", flush=True)
        ws = groups.get("wire", [])
        if ws:
            cols = []
            for k in ws:
                S = self.pieces[k]["shape"]
                (j0, j1, i0, i1), x, y = grid_of(S, 1)
                d = C.sdf.sd(S, x, y)
                m = (d < -0.6) & (er[j0:j1, i0:i1] == k) & self.full[j0:j1, i0:i1]
                cols.append(self.art[j0:j1, i0:i1][m])
            c = np.concatenate(cols)
            c = c[C.fit.lum(c) < 90] if len(c) else c      # only the line's own dark pixels (not a stripe it crosses)
            self.paints["wire"] = [round(float(v), 1) for v in np.median(c, axis=0)] if len(c) > 10 else list(P.NAVY)
            print(f"  wire colour {self.paints['wire']} from {len(c)} px", flush=True)

    def shadow_layers(self, sh):
        out = []
        for p in self.pieces:
            if p["group"] in ("slab", "band", "cell", "well"):
                continue
            out.append({"shape": {"t": "move", "of": self.shapes(p)[0], "d": [sh["dx"], sh["dy"]]}, "fill": sh["rgb"],
                        "alpha": sh["a"], "soft": sh["soft"]})
        return out

    def fit_shadow(self, boxes):
        ink = self._ink
        best = None
        cache = {}
        for dx in (0.5, 1.0, 1.5):
            for dy in (1.0, 1.5, 2.0):
                for soft in (0.7, 1.2):
                    for a in (0.3, 0.45, 0.6):
                        sh = {"dx": dx, "dy": dy, "soft": soft, "a": a, "rgb": [40, 40, 80]}
                        e, n = 0.0, 0
                        for b in boxes:
                            low = K.down(K.render(self.scene(sh, slab_only=True), b, 2, cache=cache), 2)
                            x0, y0, x1, y1 = b
                            m = (ink[y0:y1, x0:x1] > 0.6) & self.full[y0:y1, x0:x1] & self.slab_valid[y0:y1, x0:x1]
                            e += float(np.abs(low - self.art[y0:y1, x0:x1])[m].sum())
                            n += int(m.sum())
                        if best is None or e / n < best[0]:
                            best = (e / n, sh)
        print(f"  shadow {best[1]} mad {best[0]:.2f}", flush=True)
        return best[1]

    # ------------------------------------------------------------------ scene
    def scene(self, shadow=None, slab_only=False, only=None):
        """Slab, then one small item per ink shadow, then the pieces in draw order."""
        sc = []
        for p in self.pieces:
            if p["group"] == "slab":
                L = {"shape": p["shape"], "fill": self.paints["slab"], "key": p["name"]}
                if p.get("rows") or self.cfg.get("slab_rows"):
                    L["clip"] = self.slab_clip(p.get("rows"))
                sc.append(K.item([L]) | {"part": p["name"]})
        if shadow:
            for L, p in zip(self.shadow_layers(shadow), [q for q in self.pieces if q["group"] not in ("slab", "band", "cell", "well")]):
                sc.append(K.item([L]) | {"part": "shadow:" + p["name"]})
        if slab_only:
            return sc + [self.piece_item(p) for p in self.pieces if p["group"] in ("well", "band")]
        for p in self.pieces:
            if p["group"] == "slab" or (only and p["name"] not in only):
                continue
            sc.append(self.piece_item(p))
        return sc

    def slab_clip(self, rows=None):
        return {"t": "union", "of": [{"t": "rrect", "c": [400.0, (a + b) / 2], "half": [420.0, (b - a) / 2], "r": 0.0,
                                      "rot": 0.0} for a, b in (rows or self.cfg["slab_rows"])]}

    def render(self, box, s, cache=None):
        sc = self.scene(self.shadow) + self.extra_items() + self.ok_items()
        img = K.render(sc, box, s, cache=cache)
        # pictures kept as art: their own pixels pasted back (nearest), inside the measured circle
        for (cx, cy, r) in self.cfg.get("keep_art_circles", []):
            x0, y0, x1, y1 = box
            yy, xx = np.mgrid[0:img.shape[0], 0:img.shape[1]]
            fx, fy = x0 + (xx + 0.5) / s, y0 + (yy + 0.5) / s
            m = np.hypot(fx - cx, fy - cy) <= r
            if m.any():
                img[m] = self.art[np.clip(fy[m].astype(int), 0, 599), np.clip(fx[m].astype(int), 0, 799)]
        return img


def run(key):
    sh = Sheet(key)
    print(f"[{key}] gather", flush=True)
    sh.gather()
    print(f"  {len(sh.pieces)} pieces: " + ", ".join(f"{g} {sum(p['group'] == g for p in sh.pieces)}" for g in ORDER), flush=True)
    sh.fit_paints()
    sh.shadow = sh.cfg.get("shadow") or sh.fit_shadow(sh.cfg.get("shadow_boxes", []))
    json.dump({"paints": sh.paints, "W": sh.W, "shadow": sh.shadow, "fit_mad": getattr(sh, "fit_mad", {})},
              open(os.path.join(C.HERE, "geom", sh.key + "_paint.json"), "w"))
    return sh





# ---------------------------------------------------------------------------------------------------------------- output
def piece_box(sh, p):
    return K.bbox(sh.shapes(p)[0], 0)


def inside(b, crop):
    x, y, w, h = crop
    return b[0] >= x - 1 and b[1] >= y - 1 and b[2] <= x + w + 1 and b[3] <= y + h + 1


def overlaps(b, crop):
    x, y, w, h = crop
    return b[0] < x + w and b[2] > x and b[1] < y + h and b[3] > y


def edge_px(sh, mine_low, crop, pieces):
    """Largest distance between the art's and the render's navy line centres over the pills / plates of a crop (each side
    measured the same way on both, as parts.measure_pill reads them)."""
    x, y, w, h = crop
    full = sh.meas.copy()
    full[y:y + h, x:x + w] = mine_low
    full[sh.alpha < 0.5] = 255.0
    worst = []
    for p in pieces:
        if not p["group"].startswith("pill"):
            continue
        b = p["fill_box"]
        if not inside(piece_box(sh, p), crop):
            continue
        a, _ = P.measure_pill(sh.meas, b)
        m, _ = P.measure_pill(full, b)
        if a is None or m is None:
            continue
        worst.append(max(abs(u - v) for u, v in zip(a, m)))
    return (round(float(np.median(worst)), 2), round(float(max(worst)), 2)) if worst else None


def outline_px(sh, mine_low_alpha, crop):
    """Mean distance (px) between the slab's silhouette and the art's: summed coverage difference / perimeter."""
    x, y, w, h = crop
    A = sh.alpha[y:y + h, x:x + w]
    v = sh.valid[y:y + h, x:x + w]
    d = np.abs(mine_low_alpha - A)[v].sum()
    per = max(float((np.abs(np.diff(A, axis=0)) > 0.3).sum() + (np.abs(np.diff(A, axis=1)) > 0.3).sum()), 1.0)
    return round(float(d / per), 3)


def wanted():
    """Round 2: only the elements Mariano marked fix / redo (log/roadmap/P41/judge/decisions_final.json)."""
    if C.RESULTS != "results2":
        return None
    d = json.load(open(os.path.join(C.ROOT, "log", "roadmap", "P41", "judge", "decisions_final.json")))
    return {k.split("--", 1)[1] for k, v in d.items() if k.startswith("reports_slabs--") and v["verdict"] in ("fix", "redo")}


def emit(sh, s_score=2):
    crops = roster_crops(sh.name)
    cfg = sh.cfg
    rows_out = []
    cache = {}

    def mine(box, s):
        k = (tuple(box), s)
        if k not in cache:
            cache[k] = sh.render(box, s)
        return cache[k]

    scene_all = sh.scene(sh.shadow) + sh.extra_items() + sh.ok_items()
    want = wanted()
    for eid, info in crops.items():
        if want is not None and eid not in want:
            continue
        if eid in cfg.get("external", {}):
            continue
        if eid in cfg.get("kept_art", {}):
            rows_out.append({"id": eid, "status": "kept_art", "how": cfg["kept_art"][eid], "interior_mad": "", "edge_px": "",
                             "unsure": ""})
            continue
        rows = list(info["rows"])
        rows += [(f"detail {n}", b) for n, b in cfg.get("details", {}).get(eid, [])]
        path = os.path.join(C.OUT, eid + ".png")
        K.compare_png(path, sh.art, mine, rows, scale=cfg.get("scale", {}).get(eid))
        mads, edges = [], []
        for st, (x, y, w, h) in info["rows"]:
            low = K.down(mine((x, y, x + w, y + h), s_score), s_score)
            d = np.abs(low - sh.art[y:y + h, x:x + w])
            m = sh.alpha[y:y + h, x:x + w] > 0.99
            mads.append(float(d[m].mean()) if m.any() else 0.0)
            e = edge_px(sh, low, (x, y, w, h), sh.pieces)
            if e:
                edges.append(e[1])
        # the element's own pieces for its scene file
        kind = info["kind"]
        if kind == "report slab":
            items = scene_all
        else:
            names = set()
            for st, crop in info["rows"]:
                for p in sh.pieces:
                    if p["group"] == "slab":
                        continue
                    if kind == "title plate" and p["group"] != "plate":
                        continue
                    if kind == "pill" and p["group"] == "plate":
                        continue
                    if overlaps(piece_box(sh, p), crop):
                        names.add(p["name"])
            items = [it for it in scene_all if it.get("part") in names or it.get("part", "").split(":")[-1] in names]
            items += [it for it in sh.extra_items() + sh.ok_items() if any(
                overlaps(it["box"], c) for _, c in info["rows"]) and it not in items]
        json.dump({"id": eid, "sheet": sh.name, "frame": "800x600 sheet pixels", "crops": info["rows"],
                   "scene": items, "paints": sh.paints, "line_widths": sh.W, "shadow": sh.shadow},
                  open(os.path.join(C.OUT, eid + ".json"), "w"))
        rows_out.append({"id": eid, "status": "done", "how": cfg.get("how", {}).get(eid, ""),
                         "interior_mad": round(float(np.mean(mads)), 2), "edge_px": (round(max(edges), 2) if edges else ""),
                         "unsure": cfg.get("unsure", {}).get(eid, "")})
        print(f"  {eid}: mad {[round(v, 2) for v in mads]} edge {edges}", flush=True)
    os.makedirs(os.path.join(C.HERE, C.RESULTS), exist_ok=True)
    json.dump(rows_out, open(os.path.join(C.HERE, C.RESULTS, sh.key + ".json"), "w"), indent=1)
    json.dump({"paints": sh.paints, "W": sh.W, "shadow": sh.shadow, "fit_mad": getattr(sh, "fit_mad", {})},
              open(os.path.join(C.HERE, "geom", sh.key + "_paint.json"), "w"))
    return rows_out


def flat_deep(sh, depth=9.0):
    """Round 2: the slab's lilac is flat past 9 px from any edge (every sheet's fit gives 148 150 198 there), but the
    least-squares stops at 16-40 px rest on few pixels and drifted (Tournament: 165 167 207 at 21 px, which drew a light
    seam rectangle inside the foot tabs). Every stop from `depth` on takes the stop-`depth` body colour, no direction."""
    for g in ("slab", "well"):
        P_ = sh.paints.get(g)
        if not (isinstance(P_, dict) and "bevel" in P_):
            continue
        b = P_["bevel"]
        k0 = next(i for i, t in enumerate(b["stops"]) if t >= depth)
        # the body colour: the sheet's own flat lilac (the most frequent opaque lilac-family colour of the sheet)
        m = (sh.alpha > 0.99) & sh.slab_valid
        px = sh.raw[m].astype(int)
        px = px[(px[:, 2] > px[:, 0] + 30) & (px[:, 2] > px[:, 1] + 30)]
        v, c = np.unique(px, axis=0, return_counts=True)
        base = [float(t) for t in v[np.argmax(c)]] if len(v) else b["coef"][k0][0]
        for i in range(k0, len(b["stops"])):
            b["coef"][i] = [list(base)] + [[0.0, 0.0, 0.0]] * 4


def wire_colour(sh):
    """The wires' navy: median of the darkest third of the pixels inside the wire strokes (the round-1 median of every
    pixel under 90 luminance came out grey, 77 76 91, where the art's line core is 32 42 75)."""
    own, er = sh.owner()
    cols = []
    for k, p in enumerate(sh.pieces):
        if p["group"] != "wire":
            continue
        (j0, j1, i0, i1), x, y = grid_of(p["shape"], 1)
        d = C.sdf.sd(p["shape"], x, y)
        m = (d < 0.3) & (own[j0:j1, i0:i1] == k) & sh.full[j0:j1, i0:i1]
        cols.append(sh.art[j0:j1, i0:i1][m])
    if not cols:
        return
    c = np.concatenate(cols)
    if len(c) < 20:
        return
    L = C.fit.lum(c)
    c = c[L <= np.percentile(L, 33)]
    sh.paints["wire"] = [round(float(v), 1) for v in np.median(c, axis=0)]
    print("  wire colour (darkest third)", sh.paints["wire"], flush=True)


def main(key, reuse=False):
    if reuse:
        sh = Sheet(key)
        sh.gather()
        pj = json.load(open(os.path.join(C.HERE, "geom", key + "_paint.json")))
        sh.paints, sh.W, sh.shadow = pj["paints"], pj["W"], pj["shadow"]
        sh.fit_mad = pj.get("fit_mad", {})
    else:
        sh = run(key)
    if sh.cfg.get("wire_dark", True):
        wire_colour(sh)
    flat_deep(sh)
    print("  slab body past 9 px", sh.paints["slab"]["bevel"]["coef"][-1][0], flush=True)
    emit(sh)


if __name__ == "__main__":
    main(sys.argv[1], "--reuse" in sys.argv)
