"""S5.1 probe 7: for chosen bytes of arial.ttf, the glyph's kind (simple / composite), whether a composite carries its own
instructions (WE_HAVE_INSTRUCTIONS 0x100 on a component) and USE_MY_METRICS (0x200), from the glyf / loca tables.

    ~/bin/py -3.12 log/roadmap/S5/1_glyf_probe.py [file] [bytes hex ...]
"""
import importlib.util
import os
import pathlib
import struct
import sys

spec4 = importlib.util.spec_from_file_location("p4", pathlib.Path(__file__).with_name("1_adv_probe.py"))
p4 = importlib.util.module_from_spec(spec4)
spec4.loader.exec_module(p4)


def glyph(t, gi):
    long = struct.unpack_from(">h", t["head"], 50)[0]
    loca = t["loca"]
    if long:
        a, b = struct.unpack_from(">II", loca, 4 * gi)
    else:
        a, b = (2 * x for x in struct.unpack_from(">HH", loca, 2 * gi))
    return t["glyf"][a:b]


def describe(t, gi):
    g = glyph(t, gi)
    if not g:
        return "empty"
    nc = struct.unpack_from(">h", g, 0)[0]
    if nc >= 0:
        ends = struct.unpack_from(f">{nc}H", g, 10)
        il = struct.unpack_from(">H", g, 10 + 2 * nc)[0]
        return f"simple contours {nc} instructions {il}"
    off, comps, instr, mym = 10, [], False, []
    while True:
        flags, idx = struct.unpack_from(">HH", g, off)
        comps.append(idx)
        if flags & 0x200:
            mym.append(idx)
        if flags & 0x100:
            instr = True
        off += 4 + (4 if flags & 1 else 2)
        off += 2 if flags & 8 else 4 if flags & 0x40 else 8 if flags & 0x80 else 0
        if not flags & 0x20:
            break
    il = struct.unpack_from(">H", g, off)[0] if instr else 0
    return f"composite of {comps} use_my_metrics {mym} own instructions {il if instr else 'none'}"


def main():
    path = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(os.environ["WINDIR"]) / "Fonts" / "arial.ttf"
    bs = [int(x, 16) for x in sys.argv[2:]] or [0x63, 0x8e, 0x9c, 0x9e, 0xc0, 0xc7, 0xd0, 0xd1, 0xec, 0x41, 0x7a]
    t = p4.tables(path.read_bytes())
    for b in bs:
        ch = bytes([b]).decode("cp1252", "replace")
        gi = p4.cmap_lookup(t, ord(ch))
        print(f"{b:02x} U+{ord(ch):04X} glyph {gi}: {describe(t, gi)}")


if __name__ == "__main__":
    main()
