"""Turns the workers' U-rows (log/roadmap/S6/*_uncertain.tsv) into UNCERTAINTIES.md table rows
(`| ID | addr | type | description | path to resolution | status |`), written to log/roadmap/S6/uncertain_rows.md.

Accepted shapes (workers wrote four):
  header `id address kind description needed status`  (S6.1, S6.6, normalised 4_ and 5_)
  header `id topic statement status`                   (S6.2)
  header `id address kind text status`                 (S6.5 as first written)
  no header line at all, rows `id kind description`    (S6.4 as first written)
An `address` cell that is not a 0x hex address (a function name) moves into the description. Every U-row of every file is
kept: a row whose id is missing from the output is an error."""
import csv
import pathlib
import re

S6 = pathlib.Path(__file__).resolve().parent
HEX = re.compile(r"^0x[0-9a-fA-F]+$")
KINDS = {"semantic", "structural", "behavioural", "tooling"}


def rows_of(p):
    with open(p, newline="", encoding="utf-8") as fh:
        raw = [r for r in csv.reader(fh, delimiter="\t") if r]
    if not raw:
        return []
    if raw[0][0].startswith("U-"):  # no header line: id kind description
        return [{"id": r[0], "kind": r[1], "description": "\t".join(r[2:])} for r in raw]
    h = raw[0]
    return [dict(zip(h, r)) for r in raw[1:] if r[0].startswith("U-")]


def main():
    out, ids = [], []
    for p in sorted(S6.glob("*_uncertain.tsv")):
        for d in rows_of(p):
            uid = d["id"]
            ids.append(uid)
            if "topic" in d:
                addr, typ, desc, need = "", "behavioural", f"{d['topic']}: {d['statement']}", ""
            else:
                addr = d.get("address", "")
                desc = d.get("description") or d.get("text") or d.get("statement", "")
                if addr and not HEX.match(addr):
                    desc = f"{addr}: {desc}"
                    addr = ""
                typ = d.get("kind", "behavioural")
                if typ not in KINDS:  # reach / choice / open ...
                    typ = "behavioural"
                need = d.get("needed", "")
            status = d.get("status") or "open"
            out.append(f"| {uid} | {addr} | {typ} | [S6, {p.name[0]}] {desc} | {need} | {status} |")
    assert len(ids) == len(set(ids)), "duplicate U ids"
    (S6 / "uncertain_rows.md").write_text("\n".join(out) + "\n", encoding="utf-8")
    print(len(out), "rows:", ids[0], "..", ids[-1])


main()
