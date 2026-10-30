"""S3.15: the two censuses (15_census_gl.tsv = GlRaster default, 15_census_wgl.tsv = --host-gl) against each other and against I15's.

    ~/bin/py -3.12 log/roadmap/S3/15_census_cmp.py            # -> 15_census_cmp.tsv (per row), 15_census_patterns.tsv, stdout summary

Pattern = I14's px of hud_title / hud_meters / hud_message / toolbar for a FAIL row (the counts the step was given), else the row's I14
status. Checks: GlRaster census = I15's census row for row (status and every region's px; the default path must be unchanged); rows
whose status or px differ between the two backends; per pattern the statuses and px sums of both."""
import collections
import csv
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
M = HERE.parent / "merges"
REG = ("hud_title", "hud_meters", "hud_message", "toolbar", "bottom_panel", "view")


def load(p):
    return {(r["scenario"], r["checkpoint"]): r for r in csv.DictReader(open(p, encoding="utf-8"), delimiter="\t")}


def regions(r):
    out = {}
    for p in (r.get("regions") or "").split(";"):
        if "=" in p:
            k, v = p.split("=", 1)
            out[k] = int(v.split("/")[0])
    return out


def main():
    i14, i15 = load(M / "I14" / "census.tsv"), load(M / "I15" / "census.tsv")
    gl, wgl = load(HERE / "15_census_gl.tsv"), load(HERE / "15_census_wgl.tsv")
    keys = sorted(set(gl) | set(wgl))
    gl_vs_i15 = [k for k in keys if k in i15 and k in gl and (gl[k]["status"] != i15[k]["status"] or regions(gl[k]) != regions(i15[k]))]
    pat = collections.OrderedDict()
    rows_out = []
    for k in keys:
        a, b = gl.get(k), wgl.get(k)
        base = i14.get(k)
        if base and base["status"] == "FAIL":
            rb = regions(base)
            p = "/".join(str(rb.get(n, 0)) for n in REG[:4])
        else:
            p = "I14 " + (base["status"] if base else "absent")
        d = pat.setdefault(p, {"rows": 0, "gl": collections.Counter(), "wgl": collections.Counter(), "px_gl": collections.Counter(),
                               "px_wgl": collections.Counter()})
        d["rows"] += 1
        for tag, r in (("gl", a), ("wgl", b)):
            if r is None:
                d[tag]["missing"] += 1
                continue
            d[tag][r["status"]] += 1
            for n, v in regions(r).items():
                d["px_" + tag][n] += v
        if a and b and (a["status"] != b["status"] or regions(a) != regions(b)):
            ra, rb2 = regions(a), regions(b)
            rows_out.append([k[0], k[1], p, a["status"], b["status"]] +
                            [f"{ra.get(n, 0)}->{rb2.get(n, 0)}" for n in REG] + [b.get("detail", "")])
    with (HERE / "15_census_cmp.tsv").open("w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(["scenario", "checkpoint", "i14_pattern", "gl", "wgl"] + [f"{n}_gl->wgl" for n in REG] + ["wgl_detail"]) + "\n")
        for r in rows_out:
            f.write("\t".join(str(x) for x in r) + "\n")
    with (HERE / "15_census_patterns.tsv").open("w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(["i14_pattern", "rows", "gl_status", "wgl_status"] + [f"{n}_gl" for n in REG] + [f"{n}_wgl" for n in REG]) + "\n")
        for p, d in sorted(pat.items(), key=lambda kv: -kv[1]["rows"]):
            st = lambda c: ",".join(f"{s}:{n}" for s, n in sorted(c.items()))
            f.write("\t".join([p, str(d["rows"]), st(d["gl"]), st(d["wgl"])] + [str(d["px_gl"][n]) for n in REG] +
                              [str(d["px_wgl"][n]) for n in REG]) + "\n")
    tot = lambda c: dict(collections.Counter(r["status"] for r in c.values()))
    print("rows gl", len(gl), tot(gl), "| wgl", len(wgl), tot(wgl), "| I15", len(i15), tot(i15))
    print("GlRaster census vs I15: rows differing", len(gl_vs_i15), gl_vs_i15[:10])
    lost = [k for k in keys if k in gl and k in wgl and gl[k]["status"] == "PASS" and wgl[k]["status"] != "PASS"]
    print("rows differing gl vs wgl:", len(rows_out), "| PASS lost with wgl:", len(lost), lost[:10])
    for n in REG:
        print(f"  {n:12s} px gl {sum(regions(r).get(n, 0) for r in gl.values()):9d}  wgl {sum(regions(r).get(n, 0) for r in wgl.values()):9d}")


if __name__ == "__main__":
    main()
