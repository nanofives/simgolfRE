"""Remove the binaries' text from naming outputs before they are committed: quoted strings ('...' / "...") of 4+
characters in re/names/*.tsv and re/analysis/systems/*.md become `(string in the binary)`. Address citations stay.
Prints what it replaced.   py -3.12 re/tools/names_sanitize.py [files...]"""
import glob, re, sys

QUOTE = re.compile(r"""(?<![\w`])(['"])([^'"\n`]{4,}?)\1(?![\w`])""")
files = sys.argv[1:] or sorted(glob.glob("re/names/*.tsv") + glob.glob("re/analysis/systems/*.md"))
total = 0
for f in files:
    t = open(f, encoding="utf-8").read()
    hits = QUOTE.findall(t)
    if not hits:
        continue
    t = QUOTE.sub("(string in the binary)", t)
    open(f, "w", encoding="utf-8").write(t)
    total += len(hits)
    print(f"{f}: {len(hits)} -> " + "; ".join(h[1][:30] for h in hits[:6]))
print(f"{total} quoted strings replaced")
