"""S10.9: SMSG class -> our established class prefix, from the existing names rows (smsg_* files excluded).
For every existing row whose address SMSG names `Class::method`, count our prefix (before `::`, else before the first
`_`). A class maps to our prefix when that prefix has >= 2 rows and at least twice the next prefix's count, is not a
placeholder (`Ch5be88`, `Sound5b6a8`: hex address in the name); a prefix dominant for several
classes goes to the one with strictly the most rows (a tie maps none).
~/bin/py -3.12 log/roadmap/S10/9_tools/classmap.py <key>   prints the map and the clashes it resolves."""
import collections, glob, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
PLACEHOLDER = re.compile(r"[0-9a-f]{5,}$")


def classmap(key):
    mod = {"sound": "sound.dll", "jgld": "jgld.dll", "Terrain": "Terrain.dll"}[key]
    th = {}
    for l in (ROOT / f"log/roadmap/S10/pdb/{key}.procs.tsv").read_text(encoding="utf-8").splitlines():
        c = l.split("\t")
        if len(c) >= 3 and c[0].startswith("0x") and "::" in c[2]:
            th[int(c[0], 16)] = c[2].rsplit("::", 1)[0]
    cnt = collections.defaultdict(collections.Counter)
    for f in glob.glob(str(ROOT / "re/names/*.tsv")):
        if f.endswith("_globals.tsv") or pathlib.Path(f).name.startswith("smsg_"):
            continue
        for l in open(f, encoding="utf-8").read().splitlines()[1:]:
            c = l.split("\t")
            if len(c) != 6 or c[0] != mod or c[2].startswith(("dynInit", "atexit")):
                continue
            a = int(c[1], 16) & 0xFFFFFF
            if a in th:
                ours = c[2].rsplit("::", 1)[0] if "::" in c[2] else c[2].split("_")[0]
                cnt[th[a]][ours] += 1
    dom = {}
    for cls, c in cnt.items():
        top = c.most_common(2)
        p, n = top[0]
        second = top[1][1] if len(top) > 1 else 0
        if n >= 2 and n >= 2 * second and not PLACEHOLDER.search(p):
            dom[cls] = (p, n)
    out = {}
    for cls, (p, n) in dom.items():
        rivals = [m for c2, (p2, m) in dom.items() if p2 == p and c2 != cls]
        if all(n > m for m in rivals):
            out[cls] = p
    return out, cnt


if __name__ == "__main__":
    m, cnt = classmap(sys.argv[1])
    for cls, p in sorted(m.items()):
        print(f"{cls:28} -> {p:22} {dict(cnt[cls])}")
