r"""Stream G2: build re/model/sve_fields.tsv (the .sve field table) from re/tools/sve.py and the merged address map re/model/addresses.tsv.

One row per field of the save format, in file order: the 100-byte header (two for championship files), the 86 fields of
FUN_0040afa0 (0x0040afa0..0x0040b492) and the three tail blocks. Each field is mapped onto model symbols by ADDRESS (the
streams' sve_field columns are compared, not trusted: e2 wrote VAs there, f wrote `24,35` / `tailA`):

  exact    one row with the field's VA and size
  slice    one row whose array contains the field on element boundaries: symbol[k:k+n]
  compose  rows lying inside the field (members at offsets; untyped gaps listed)
  none     no row

Symbols whose name embeds their own address (g_sve561250, f_w_4c2e14) count as placeholders, not names.

    ~/bin/py -3.12 re/tools/model_sve_fields.py          # writes re/model/sve_fields.tsv, log/p1/g2/sve_fields.json
"""
import csv
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import sve  # noqa: E402

STREAMS = ["b", "c", "d", "e1", "e2", "f"]
OUT = ROOT / "re" / "model" / "sve_fields.tsv"


def num(s):
    s = (s or "").strip()
    if not s or s == "-":
        return None
    try:
        return int(s, 0)
    except ValueError:
        return None


def rows():
    """rows of the MERGED address map (re/model/addresses.tsv, built by re/tools/model_merge.py from the six stream maps and
    the integration fixes); the stream column keeps the owning stream for tie-breaks."""
    out = []
    with (ROOT / "re" / "model" / "addresses.tsv").open(newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f, delimiter="	"):
            va, size = num(r.get("va")), num(r.get("size"))
            if va is None or size is None or (r.get("module") or "golf_clean.exe") != "golf_clean.exe":
                continue
            out.append({"stream": r.get("stream", ""), "symbol": r["symbol"], "label": r["symbol"], "va": va, "size": size,
                        "type": r.get("element_type", ""), "count": num(r.get("count")), "stride": num(r.get("stride")),
                        "sve": r.get("sve_field", "-"), "conf": r.get("confidence", "")})
    return out


def stream_rank(r):
    return STREAMS.index(r["stream"]) if r["stream"] in STREAMS else len(STREAMS)


def names_globals():
    out = {}
    for p in sorted((ROOT / "re" / "names").glob("*_globals.tsv")):
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines()[1:]:
            c = line.split("\t")
            if len(c) >= 3 and c[0] == "golf_clean.exe":
                v = num(c[1])
                if v is not None:
                    out.setdefault(v, []).append((c[2], p.name))
    return out


def placeholder(sym, va):
    return re.search(r"[0-9a-fA-F]{6,8}", sym) is not None and (f"{va:x}" in sym.lower() or f"{va:06x}" in sym.lower())


def fields():
    """(key, part, va, size, present_if) in file order."""
    out = [("H0", "header", 0x0051A068, sve.HEADER_LEN, "always"),
           ("H1", "header2", 0x0051A068, sve.HEADER_LEN, "championship file only (FUN_0040b9b0 champ != 0)")]
    for i, (va, n) in enumerate(sve.FIELDS):
        out.append((str(i), "body", va, n, "always"))
    out.append(("tailA", "tailA", sve.TAIL_A[1][0][0], sve.TAIL_A[1][0][1], "flags & 0x40000000 (field 70)"))
    for k, (va, n) in enumerate(sve.TAIL_B[1]):
        out.append((f"tailB{k}", "tailB", va, n, "flags & 0x00200000 (field 70)"))
    return out


def stride_of(r):
    st = r["stride"] or 0
    if st <= 0 and r["count"]:
        st = r["size"] // r["count"]
    return st


def map_field(va, size, allrows):
    end = va + size
    ov = [r for r in allrows if r["va"] < end and r["va"] + r["size"] > va]
    exact = [r for r in ov if r["va"] == va and r["size"] == size]
    inside = [r for r in ov if r["va"] >= va and r["va"] + r["size"] <= end]
    cover = [r for r in ov if r["va"] <= va and r["va"] + r["size"] >= end and r not in exact]
    other = [r for r in ov if r not in exact and r not in inside and r not in cover]
    m = {"overlapping": ov}
    if exact:
        exact.sort(key=stream_rank)
        m.update(mapping="exact", members=[(exact[0], 0, None)], alternates=exact[1:] + [r for r in ov if r not in exact])
        return m
    aligned = []
    for r in cover:
        st = stride_of(r)
        if st and (va - r["va"]) % st == 0 and size % st == 0:
            aligned.append((r, (va - r["va"]) // st, size // st))
    if aligned:
        aligned.sort(key=lambda t: -stride_of(t[0]))
        r, k, n = aligned[0]
        m.update(mapping="slice", members=[(r, 0, (k, k + n))], alternates=[x for x in ov if x is not r])
        return m
    if inside:
        inside.sort(key=lambda r: (r["va"], -r["size"]))
        chosen, pos = [], va
        for r in inside:
            if r["va"] >= pos:
                chosen.append((r, r["va"] - va, None))
                pos = r["va"] + r["size"]
        m.update(mapping="compose", members=chosen, alternates=[x for x in ov if all(x is not c[0] for c in chosen)])
        return m
    m.update(mapping="none", members=[], alternates=ov)
    return m


def member_text(r, off, sl):
    s = r["symbol"] + (f"[{sl[0]}:{sl[1]}]" if sl else "")
    return s if off == 0 else f"{s}@+0x{off:x}"


def main():
    allrows = rows()
    ng = names_globals()
    out, js = [], []
    body_off = 0
    for key, part, va, size, present in fields():
        m = map_field(va, size, allrows)
        mem = m["members"]
        typed = 0
        for r, off, sl in mem:
            typed += (sl[1] - sl[0]) * stride_of(r) if sl else r["size"]
        names = [r["symbol"] for r, _o, _s in mem]
        if not mem:
            status = "unnamed"
        elif typed < size:
            status = "partial"
        elif all(placeholder(r["label"], r["va"]) for r, _o, _s in mem):
            status = "placeholder"
        else:
            status = "named"
        old = sve.NAMES.get(va) if part in ("body", "tailA", "tailB") else None
        if part == "header":
            fo, rule = "0x0", "0 (one 100-byte buffer; championship files have a second at 100)"
        elif part == "header2":
            fo, rule = "0x64", "100, championship files only; every later offset moves by 100"
        else:
            fo = f"0x{sve.HEADER_LEN + body_off:x}"
            if part == "body":
                rule = "100 * headers + sum of the sizes of the body fields before it"
            elif part == "tailA":
                rule = "after field 85"
            else:
                rule = "after field 85 and tail A when present" if key == "tailB0" else "after tailB0"
            body_off += size
        claims = sorted({f"{r['stream']}:{r['sve']}" for r in m["overlapping"] if r["sve"] not in ("-", "")})
        alts = [f"{r['stream']}:{r['symbol']}(0x{r['va']:x}+0x{r['size']:x})" for r in m["alternates"]]
        gnames = sorted({f"{n}({f})" for a, lst in ng.items() if va <= a < va + size for n, f in lst})
        row = {
            "index": key, "part": part, "va": f"0x{va:08x}", "size": f"0x{size:x}", "file_offset": fo,
            "offset_rule": rule, "present_if": present, "sve_py_name": old or "-",
            "old_status": ("named" if old else "unnamed") if part not in ("header", "header2") else "-",
            "mapping": m["mapping"], "model_symbols": ";".join(member_text(r, o, s) for r, o, s in mem) or "-",
            "element_types": ";".join(r["type"] for r, _o, _s in mem) or "-",
            "streams": ",".join(sorted({r["stream"] for r, _o, _s in mem})) or "-",
            "typed_bytes": f"0x{typed:x}", "status": status,
            "stream_sve_claims": ",".join(claims) or "-", "other_rows": ";".join(alts) or "-",
            "names_globals": ";".join(gnames) or "-", "labels": ";".join(r["label"] for r, _o, _s in mem) or "-",
        }
        out.append(row)
        js.append({**row, "members": [{"symbol": r["symbol"], "stream": r["stream"], "offset": o,
                                       "slice": list(s) if s else None, "type": r["type"], "count": r["count"],
                                       "stride": stride_of(r), "size": r["size"]} for r, o, s in mem]})
    cols = list(out[0])
    with OUT.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=cols, delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(out)
    (ROOT / "log" / "p1" / "g2" / "sve_fields.json").write_text(json.dumps(js, indent=1))
    from collections import Counter
    print(OUT, len(out), "rows")
    print("mapping", Counter(r["mapping"] for r in out))
    print("status", Counter(r["status"] for r in out))
    for r in out:
        if r["status"] != "named" or r["other_rows"] != "-":
            print(r["index"], r["va"], r["size"], r["mapping"], r["status"], r["model_symbols"], "| other:", r["other_rows"],
                  "| names:", r["names_globals"])


if __name__ == "__main__":
    main()
