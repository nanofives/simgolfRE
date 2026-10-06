"""Validate naming outputs: field counts, subsystems, unique names/addrs, banned words.   py -3.12 re/tools/names_validate.py ID..."""
import re, sys

SUBS = set("boot frontend render terrain course golfer economy ai audio video input save ui sim net util crt unknown".split())
BANNED = re.compile(r"\b(probably|likely|seems to|appears to|I think|presumably|might be|maybe)\b", re.I)
bad = 0
for i in sys.argv[1:]:
    for f, n in ((f"re/names/{i}.tsv", 6), (f"re/names/{i}_globals.tsv", 5)):
        try:
            lines = open(f, encoding="utf-8").read().splitlines()
        except FileNotFoundError:
            print(f"{f}: missing"); continue
        names, addrs = set(), set()
        for k, l in enumerate(lines[1:], 2):
            c = l.split("\t")
            err = []
            if len(c) != n: err.append(f"{len(c)} fields")
            if n == 6 and len(c) > 3 and c[3] not in SUBS: err.append(f"subsystem {c[3]}")
            if len(c) > 2 and c[2] in names: err.append(f"dup name {c[2]}")
            if len(c) > 1 and c[1] in addrs: err.append(f"dup addr {c[1]}")
            if BANNED.search(l): err.append("banned word")
            if err:
                bad += 1; print(f"{f}:{k}: {', '.join(err)}")
            if len(c) > 2: names.add(c[2]); addrs.add(c[1])
        print(f"{f}: {len(lines)-1} rows")
    md = open(f"re/analysis/systems/{i}.md", encoding="utf-8").read()
    for m in BANNED.finditer(md):
        bad += 1; print(f"systems/{i}.md: banned '{m.group(0)}'")
print("bad", bad)
