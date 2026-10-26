"""S5.1 probe 4: GDI's single-character advances (GetTextExtentPoint32A of one byte, 0x20..0xff) for the faces and sizes
the game uses, against two rules computed from the font file alone: the hdmx device width at the ppem (when the table has
that ppem) and the linearly scaled advance round(advanceWidth * ppem / unitsPerEm). Reports, per face and size, how many
of the 224 bytes each rule gets right, and the head flags bit 4 ("instructions may alter advance width").

    ~/bin/py -3.12 log/roadmap/S5/1_adv_probe.py
"""
import ctypes
import ctypes.wintypes as W
import importlib.util
import os
import pathlib
import struct

spec = importlib.util.spec_from_file_location("p1", pathlib.Path(__file__).with_name("1_gdi_probe.py"))
p1 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p1)
gdi = p1.gdi
gdi.GetTextExtentPoint32A.argtypes = [W.HDC, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(W.SIZE)]
FONTS = pathlib.Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts"


def tables(b):
    n = struct.unpack_from(">H", b, 4)[0]
    t = {}
    for i in range(n):
        tag, _, off, ln = struct.unpack_from(">4sIII", b, 12 + 16 * i)
        t[tag.decode("latin-1")] = b[off:off + ln]
    return t


def cmap_lookup(t, u):
    c = t["cmap"]
    n = struct.unpack_from(">H", c, 2)[0]
    for i in range(n):
        pid, eid, off = struct.unpack_from(">HHI", c, 4 + 8 * i)
        if pid == 3 and eid in (1, 0) and struct.unpack_from(">H", c, off)[0] == 4:
            cc = u if eid == 1 else 0xF000 + u
            segx2 = struct.unpack_from(">H", c, off + 6)[0]
            ends = struct.unpack_from(f">{segx2 // 2}H", c, off + 14)
            starts = struct.unpack_from(f">{segx2 // 2}H", c, off + 16 + segx2)
            deltas = struct.unpack_from(f">{segx2 // 2}h", c, off + 16 + 2 * segx2)
            roff_at = off + 16 + 3 * segx2
            for k in range(segx2 // 2):
                if starts[k] <= cc <= ends[k]:
                    ro = struct.unpack_from(">H", c, roff_at + 2 * k)[0]
                    if ro == 0:
                        return (cc + deltas[k]) & 0xFFFF
                    g = struct.unpack_from(">H", c, roff_at + 2 * k + ro + 2 * (cc - starts[k]))[0]
                    return (g + deltas[k]) & 0xFFFF if g else 0
            return 0
    return 0


def main():
    dc = gdi.CreateCompatibleDC(None)
    inst = p1.INSTALL
    for f in ("manu3_.ttf", "klepto__.ttf"):
        gdi.AddFontResourceExA(str(inst / f).encode(), p1.FR_PRIVATE, None)
    cases = [("Arial", FONTS / "arial.ttf", [9, 10, 18]), ("Times New Roman", FONTS / "times.ttf", [12]),
             ("Klepto ITC TT", inst / "klepto__.ttf", [18, 24]), ("Manual SSi Bold", inst / "manu3_.ttf", [14, 15, 16, 18, 20, 24])]
    for face, path, sizes in cases:
        b = path.read_bytes()
        t = tables(b)
        upem = struct.unpack_from(">H", t["head"], 18)[0]
        flags = struct.unpack_from(">H", t["head"], 16)[0]
        nhm = struct.unpack_from(">H", t["hhea"], 34)[0]
        ng = struct.unpack_from(">H", t["maxp"], 4)[0]
        hdmx = {}
        if "hdmx" in t:
            h = t["hdmx"]
            nrec, recsz = struct.unpack_from(">hi", h, 2)
            for r in range(nrec):
                o = 8 + r * recsz
                hdmx[h[o]] = h[o + 2:o + 2 + ng]
        cp = bytes(range(0x20, 0x100)).decode("cp1252", "replace")
        for size in sizes:
            lf = p1.LOGFONTA()
            lf.lfHeight = -size
            lf.lfOutPrecision = 7
            lf.lfFaceName = face.encode()
            hf = gdi.CreateFontIndirectA(ctypes.byref(lf))
            old = gdi.SelectObject(dc, hf)
            ok_h = ok_l = n = 0
            bad = []
            for i, ch in enumerate(cp):
                byte = 0x20 + i
                sz = W.SIZE()
                gdi.GetTextExtentPoint32A(dc, bytes([byte]), 1, ctypes.byref(sz))
                g = cmap_lookup(t, ord(ch)) if ch != "\ufffd" else 0
                aw = struct.unpack_from(">H", t["hmtx"], 4 * min(g, nhm - 1))[0]
                lin = (aw * size * 2 + upem) // (2 * upem)
                hd = hdmx[size][g] if size in hdmx else None
                n += 1
                ok_l += lin == sz.cx
                ok_h += hd == sz.cx
                if lin != sz.cx and len(bad) < 8:
                    bad.append(f"0x{byte:02x} gdi {sz.cx} lin {lin} hdmx {hd} aw {aw}")
            print(f"{face} {size}: flags bit4 {(flags >> 4) & 1} hdmx-has-ppem {size in hdmx}: hdmx right {ok_h}/{n},"
                  f" linear right {ok_l}/{n}; linear misses {bad}")
            gdi.SelectObject(dc, old)
            gdi.DeleteObject(hf)


if __name__ == "__main__":
    main()
