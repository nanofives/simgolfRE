"""S10.6: from 6_extent.tsv write the lines the orchestrator applies (nothing here touches a shared file):
6_boundaries_sound.tsv (rows for re/match/boundaries.tsv), 6_setbodies_args.txt (ghidra/scripts/SetBodies.java arguments),
6_staged_canonical.tsv (source, MATCH line, extent) of the new 100% lines."""
import csv
import pathlib

D = pathlib.Path(__file__).resolve().parents[1]
rows = sorted(csv.DictReader((D / "6_extent.tsv").open(encoding="utf-8"), delimiter="\t"), key=lambda r: int(r["va"], 16))
good = [r for r in rows if r["status"] == "100" and r["decodes_exact"] == "yes" and not r["inside_listed"]]
with (D / "6_boundaries_sound.tsv").open("w", encoding="utf-8", newline="") as f:
    for r in good:
        why = "S10.6: SMSG sound.pdb procedure " + r["pdb_name"] + ("" if r["extent"] == r["pdb_size"] else " (extent ends at the last ret; the PDB size adds the switch tables)")
        f.write(f"sound.dll\t{r['va']}\t{r['extent']}\t{why}\n")
(D / "6_setbodies_args.txt").write_text(" ".join(f"{r['va'][2:]}:{r['extent']}" for r in good) + "\n")
with (D / "6_staged_canonical.tsv").open("w", encoding="utf-8", newline="") as f:
    for r in good:
        f.write(f"{r['source']}\t// MATCH: sound.dll {r['va']} {r['symbol']}\t{r['extent']}\t{r['score_sp5']}\n")
print(len(good), "of", len(rows))
