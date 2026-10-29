"""I16: append U-1850..U-1897 from log/roadmap/S10/*_uncertain.tsv to UNCERTAINTIES.md (each id once)."""
import re, sys
from pathlib import Path

root = Path(__file__).resolve().parents[4]
unc = root / "UNCERTAINTIES.md"
text = unc.read_text(encoding="utf-8")
have = set(re.findall(r"^\| (U-\d{4}) \|", text, re.M))
rows, seen = [], set()
for k in range(1, 11):
    p = root / f"log/roadmap/S10/{k}_uncertain.tsv"
    for i, line in enumerate(p.read_text(encoding="utf-8").splitlines()):
        if i == 0 or not line.strip():
            continue
        cols = line.split("\t")
        if len(cols) != 6:
            sys.exit(f"{p.name}:{i+1}: {len(cols)} columns")
        uid = cols[0].strip()
        if not re.fullmatch(r"U-18[5-9]\d", uid):
            sys.exit(f"bad id {uid}")
        if uid in seen or uid in have:
            print("skip duplicate", uid)
            continue
        seen.add(uid)
        cells = [c.strip().replace("|", "\\|") for c in cols]
        rows.append((uid, "| " + " | ".join(cells) + " |"))
rows.sort()
print(len(rows), "rows", rows[0][0], "..", rows[-1][0])
if "--write" in sys.argv:
    if not text.endswith("\n"):
        text += "\n"
    text += "\n".join(r for _, r in rows) + "\n"
    unc.write_text(text, encoding="utf-8")
    print("written")
