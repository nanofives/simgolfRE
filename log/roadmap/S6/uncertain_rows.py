"""Turns the workers' U-rows (log/roadmap/S6/*_uncertain.tsv, three header shapes) into UNCERTAINTIES.md table rows
(`| ID | addr | type | description | path to resolution | status |`), written to log/roadmap/S6/uncertain_rows.md."""
import csv
import pathlib

S6 = pathlib.Path(__file__).resolve().parent
out = []
for p in sorted(S6.glob("*_uncertain.tsv")):
    with open(p, newline="", encoding="utf-8") as fh:
        rows = list(csv.reader(fh, delimiter="\t"))
    h = rows[0]
    for r in rows[1:]:
        if not r or not r[0].startswith("U-"):
            continue
        d = dict(zip(h, r))
        d.update({k: v for k, v in zip(range(len(r)), r)})
        uid = d["id"]
        if "topic" in d:
            addr, typ = "", "behavioural"
            desc = f"{d['topic']}: {d['statement']}"
            need = ""
        else:
            addr = d.get("address", "")
            typ = d.get("kind", "behavioural")
            if typ in ("reach", "choice", "open", "tooling"):
                typ = "behavioural"
            desc = d.get("description", "") or d.get("statement", "")
            need = d.get("needed", "")
        status = d.get("status", "open") or "open"
        out.append(f"| {uid} | {addr} | {typ} | [S6, {p.name[0]}] {desc} | {need} | {status} |")
(S6 / "uncertain_rows.md").write_text("\n".join(out) + "\n", encoding="utf-8")
print(len(out), "rows")
