"""S5.1: family (name ID 1) / subfamily (2) / full name (4) of every .ttf in the host font directory, Windows platform
English record (3, 1, 0x409) else the first; prints families with one face whose full name differs from the family.
    ~/bin/py -3.12 log/roadmap/S5/1_fontdir_names.py"""
import os, pathlib, struct, collections
def names(b, base=0):
    n = struct.unpack_from(">H", b, base + 4)[0]
    for i in range(n):
        t, _, off, ln = struct.unpack_from(">4sIII", b, base + 12 + 16 * i)
        if t == b"name":
            _, cnt, so = struct.unpack_from(">HHH", b, off)
            out = {}
            for k in range(cnt):
                pid, eid, lid, nid, l, o = struct.unpack_from(">6H", b, off + 6 + 12 * k)
                if pid == 3 and eid in (0, 1) and lid == 0x409:
                    out[nid] = b[off + so + o: off + so + o + l].decode("utf-16-be", "replace")
            return out
    return {}
fam = collections.defaultdict(list)
d = pathlib.Path(os.environ["WINDIR"]) / "Fonts"
for p in sorted(d.glob("*.ttf")):
    try:
        b = p.read_bytes(); nm = names(b)
    except Exception:
        continue
    fam[nm.get(1, "").lower()].append((p.name, nm.get(2), nm.get(4)))
for f, v in sorted(fam.items()):
    if len(v) == 1 and v[0][2] and v[0][2].lower() != f:
        print(f, v)
