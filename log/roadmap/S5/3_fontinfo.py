"""S5.3: table directory, name ids 1/2/4, gasp ranges, head flags of the faces the label corpus uses (read-only)."""
import struct
import sys

FILES = ["original/KLEPTO__.TTF", "original/manu3_.TTF", "C:/Windows/Fonts/arialbd.ttf", "C:/Windows/Fonts/arial.ttf",
         "C:/Windows/Fonts/times.ttf", "C:/Windows/Fonts/timesbd.ttf", "C:/Windows/Fonts/comic.ttf"]


def tables(b):
    n = struct.unpack(">H", b[4:6])[0]
    t = {}
    for i in range(n):
        tag, _, off, ln = struct.unpack(">4sIII", b[12 + 16 * i:28 + 16 * i])
        t[tag.decode("latin1")] = (off, ln)
    return t


def names(b, t):
    off, _ = t["name"]
    _, count, so = struct.unpack(">HHH", b[off:off + 6])
    out = {}
    for i in range(count):
        pid, eid, lid, nid, ln, o = struct.unpack(">6H", b[off + 6 + 12 * i:off + 18 + 12 * i])
        raw = b[off + so + o:off + so + o + ln]
        if nid in (1, 2, 4, 16, 17) and pid == 3 and lid == 0x409:
            out[nid] = raw.decode("utf-16-be")
    return out


for f in (sys.argv[1:] or FILES):
    b = open(f, "rb").read()
    t = tables(b)
    print(f, sorted(t))
    print("  names", names(b, t))
    if "gasp" in t:
        off, _ = t["gasp"]
        ver, n = struct.unpack(">HH", b[off:off + 4])
        print("  gasp v%d" % ver, [struct.unpack(">HH", b[off + 4 + 4 * i:off + 8 + 4 * i]) for i in range(n)])
    off, _ = t["head"]
    upem = struct.unpack(">H", b[off + 18:off + 20])[0]
    flags = struct.unpack(">H", b[off + 16:off + 18])[0]
    print("  upem", upem, "head.flags 0x%04x" % flags, "fpgm" in t, "prep" in t, "cvt " in t,
          "EBLC" in t)
    if "OS/2" in t:
        off, _ = t["OS/2"]
        wt = struct.unpack(">H", b[off + 4:off + 6])[0]
        fs = struct.unpack(">H", b[off + 62:off + 64])[0]
        print("  OS/2 weight", wt, "fsSelection 0x%x" % fs)
