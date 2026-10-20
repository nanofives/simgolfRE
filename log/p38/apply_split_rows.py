"""Replace rows of re/port/commands_split.tsv by address with rows given in a TSV file (parent merge helper, phase 3.8)."""
import sys
from pathlib import Path

split = Path("re/port/commands_split.tsv")
new = {}
for line in Path(sys.argv[1]).read_text(encoding="utf-8").splitlines():
    if line.strip():
        new[line.split("\t")[0]] = line
raw = split.read_bytes()
nl = "\r\n" if b"\r\n" in raw else "\n"
out, seen = [], set()
for line in raw.decode("utf-8").splitlines():
    a = line.split("\t")[0]
    if a in new:
        out.append(new[a]); seen.add(a)
    else:
        out.append(line)
missing = set(new) - seen
if missing:
    sys.exit(f"rows not found: {sorted(missing)}")
split.write_bytes((nl.join(out) + nl).encode("utf-8"))
print(f"replaced {len(seen)} rows")
