"""map compiler errors of a converted file to its blocks: 5_errmap.py <file.cpp> <compile log>"""
import re, sys, collections
src = open(sys.argv[1], encoding="utf-8").read().split("\n")
log = open(sys.argv[2], encoding="latin-1").read()
blocks = []
for i, ln in enumerate(src, 1):
    m = re.match(r"// ---- (\S+)", ln)
    if m:
        blocks.append((i, m.group(1)))
def blk(n):
    r = None
    for i, nm in blocks:
        if i <= n: r = nm
    return r
c = collections.Counter()
for m in re.finditer(r"\((\d+)\): (error|warning) (C\d+)", log):
    c[(blk(int(m.group(1))), m.group(2))] += 1
for (b, k), n in sorted(c.items(), key=lambda x: str(x)):
    print(b, k, n)
print(len(blocks), "blocks;", len({b for (b, k) in c if k == 'error'}), "with errors")
