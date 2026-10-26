"""S5.1 probe 6: candidate advance rules against GDI's single-byte extents, for every face and size the game's 15 FontRefs
use. Inputs per byte from log/roadmap/S5/1_ftprobe (FreeType v35 mono hinted advance, composite flag, linear advance in
16.16) and the font file (head flags bit 4, hdmx). Rules (the five undefined code page 1252 bytes give 0 in all, as GDI
does, 1_adv_probe.log):
  R0  hdmx width when the table has the ppem, else the v35 hinted advance
  R1  R0, but a composite glyph without hdmx takes the linear advance rounded half up
  R2  R1, and a face whose head flags bit 4 is clear takes the linear advance for every glyph without hdmx
  R3  hdmx when it has the ppem; else linear when the head flags bit 4 is clear, or when the face has an LTSH table and
      ppem >= its yPels for the glyph; else the v35 hinted advance

    ~/bin/py -3.12 log/roadmap/S5/1_rule_probe.py <ftprobe.exe>
"""
import ctypes
import ctypes.wintypes as W
import importlib.util
import os
import pathlib
import struct
import subprocess
import sys

spec = importlib.util.spec_from_file_location("p1", pathlib.Path(__file__).with_name("1_gdi_probe.py"))
p1 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p1)
spec4 = importlib.util.spec_from_file_location("p4", pathlib.Path(__file__).with_name("1_adv_probe.py"))
p4 = importlib.util.module_from_spec(spec4)
spec4.loader.exec_module(p4)
gdi = p1.gdi
gdi.GetTextExtentPoint32A.argtypes = [W.HDC, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(W.SIZE)]
FONTS = pathlib.Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts"
UNDEF = {0x81, 0x8d, 0x8f, 0x90, 0x9d}


def main():
    exe = sys.argv[1]
    inst = p1.INSTALL
    for f in ("manu3_.ttf", "klepto__.ttf"):
        gdi.AddFontResourceExA(str(inst / f).encode(), p1.FR_PRIVATE, None)
    dc = gdi.CreateCompatibleDC(None)
    cases = [("Arial", FONTS / "arial.ttf", [9, 10, 18], 0), ("Arial", FONTS / "ariali.ttf", [100], 1),
             ("Times New Roman", FONTS / "times.ttf", [12], 0),
             ("Klepto ITC TT", inst / "klepto__.ttf", [18, 24], 0),
             ("Manual SSi Bold", inst / "manu3_.ttf", [14, 15, 16, 18, 20, 24], 0)]
    for face, path, sizes, italic in cases:
        b = path.read_bytes()
        t = p4.tables(b)
        flags = struct.unpack_from(">H", t["head"], 16)[0]
        ng = struct.unpack_from(">H", t["maxp"], 4)[0]
        hdmx = {}
        if "hdmx" in t:
            h = t["hdmx"]
            nrec, recsz = struct.unpack_from(">hi", h, 2)
            for r in range(nrec):
                o = 8 + r * recsz
                hdmx[h[o]] = h[o + 2:o + 2 + ng]
        ltsh = t["LTSH"][4:] if "LTSH" in t else None
        for size in sizes:
            lf = p1.LOGFONTA()
            lf.lfHeight = -size
            lf.lfItalic = italic
            lf.lfOutPrecision = 7
            lf.lfFaceName = face.encode()
            hf = gdi.CreateFontIndirectA(ctypes.byref(lf))
            old = gdi.SelectObject(dc, hf)
            out = subprocess.run([exe, str(path), str(size)], capture_output=True, text=True, check=True).stdout
            right = [0, 0, 0, 0]
            miss = [[], [], [], []]
            for line in out.splitlines():
                parts = line.split()
                byte, gi, hinted = int(parts[0], 16), int(parts[1]), int(parts[2])
                comp = parts[-2] == "c1"
                lin = (int(parts[-1][1:]) + 0x8000) >> 16
                sz = W.SIZE()
                gdi.GetTextExtentPoint32A(dc, bytes([byte]), 1, ctypes.byref(sz))
                hd = hdmx[size][gi] if size in hdmx else None
                r0 = 0 if byte in UNDEF else (hd if hd is not None else hinted)
                r1 = 0 if byte in UNDEF else (hd if hd is not None else (lin if comp else hinted))
                r2 = 0 if byte in UNDEF else (hd if hd is not None else (lin if (comp or not (flags >> 4) & 1) else hinted))
                lt = ltsh is not None and gi < len(ltsh) and size >= ltsh[gi]
                r3 = 0 if byte in UNDEF else (hd if hd is not None else (lin if (lt or not (flags >> 4) & 1) else hinted))
                for k, r in enumerate((r0, r1, r2, r3)):
                    if r == sz.cx:
                        right[k] += 1
                    elif len(miss[k]) < 10:
                        miss[k].append(f"{byte:02x}{'c' if comp else ''}:{r}/{sz.cx} h{hinted} l{lin}")
            print(f"{path.name} {size} bit4 {(flags >> 4) & 1} hdmx {size in hdmx}: R0 {right[0]}/224 R1 {right[1]}/224 "
                  f"R2 {right[2]}/224 R3 {right[3]}/224")
            print("    R2 misses", miss[2])
            print("    R3 misses", miss[3])
            gdi.SelectObject(dc, old)
            gdi.DeleteObject(hf)


if __name__ == "__main__":
    main()
