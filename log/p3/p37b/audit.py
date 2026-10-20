"""3.7b write audit: for each UI straddler, every data address its OWN body writes, the object and class, and the
functions that READ that address. A write is an EFFECT candidate when some reader is not a drawing function (names
matching draw*/show*Screen excluded? no: readers are listed and classified by hand in classify.tsv).
Output: log/p3/p37b/audit.tsv (addr, name, written va, object+off, class, readers)"""
import sys, pathlib, csv, re, bisect, collections
ROOT = pathlib.Path(__file__).resolve().parents[3]; sys.path.insert(0, str(ROOT / "re" / "tools"))
import model_scan
objs = []
for line in (ROOT / "port/include/gamestate_meta.inc").read_text().splitlines():
    m = re.match(r"SG_OBJECT\((\w+),\s*(0x[0-9a-fA-F]+|\d+),\s*(0x[0-9a-fA-F]+|\d+),\s*(\w+)", line)
    if m: objs.append((int(m.group(2), 0) + 0x4C1000, int(m.group(3), 0), m.group(1), m.group(4)))
objs.sort(); st = [o[0] for o in objs]
def obj(v):
    i = bisect.bisect_right(st, v) - 1
    if i >= 0 and objs[i][0] <= v < objs[i][0] + objs[i][1]:
        return f"{objs[i][2]}+{v - objs[i][0]:#x}", objs[i][3]
    return hex(v), "gap"
fns, refs = model_scan.scan_cached()
name = {}
for r in csv.reader(open(ROOT / "hooks.csv", encoding="utf-8")):
    try: name[int(r[0], 16)] = r[2]
    except (ValueError, IndexError): pass
readers = collections.defaultdict(set)
for r in refs:
    if r.acc and "r" in r.acc:
        readers[r.addr].add(name.get(r.fn, hex(r.fn)))
sel = [r for r in csv.DictReader(open(ROOT / "re/port/scope_straddlers.tsv"), delimiter="\t") if r["port_module"] == "ui"]
w = csv.writer(open(ROOT / "log/p3/p37b/audit.tsv", "w", newline=""), delimiter="\t")
w.writerow(["addr", "name", "va", "object", "class", "readers"])
for s in sel:
    fa = int(s["addr"], 16)
    vas = sorted({r.addr for r in refs if r.fn == fa and r.acc and "w" in r.acc})
    for v in vas:
        o, c = obj(v)
        w.writerow([s["addr"], s["name"], hex(v), o, c, ";".join(sorted(readers.get(v, set()) - {s["name"]}))])
