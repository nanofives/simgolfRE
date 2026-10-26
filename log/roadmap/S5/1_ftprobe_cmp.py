"""S5.1 probe 5 driver: runs log/roadmap/S5/1_ftprobe (FreeType advances under seven load modes) for the faces and
sizes the game uses and counts, per mode, the bytes 0x20..0xff whose advance equals GDI's single-byte extent.

    ~/bin/py -3.12 log/roadmap/S5/1_ftprobe_cmp.py <ftprobe.exe>
"""
import ctypes
import ctypes.wintypes as W
import importlib.util
import os
import pathlib
import subprocess
import sys

spec = importlib.util.spec_from_file_location("p1", pathlib.Path(__file__).with_name("1_gdi_probe.py"))
p1 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p1)
gdi = p1.gdi
gdi.GetTextExtentPoint32A.argtypes = [W.HDC, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(W.SIZE)]
FONTS = pathlib.Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts"
MODES = ["v35mono", "v35normal", "v35light", "v40mono", "v40normal", "linear", "v35mono_computed"]


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
        for size in sizes:
            lf = p1.LOGFONTA()
            lf.lfHeight = -size
            lf.lfItalic = italic
            lf.lfOutPrecision = 7
            lf.lfFaceName = face.encode()
            hf = gdi.CreateFontIndirectA(ctypes.byref(lf))
            old = gdi.SelectObject(dc, hf)
            out = subprocess.run([exe, str(path), str(size)], capture_output=True, text=True, check=True).stdout
            right = [0] * len(MODES)
            miss = {m: [] for m in MODES}
            n = 0
            for line in out.splitlines():
                parts = line.split()
                b = int(parts[0], 16)
                sz = W.SIZE()
                gdi.GetTextExtentPoint32A(dc, bytes([b]), 1, ctypes.byref(sz))
                n += 1
                for k, m in enumerate(MODES):
                    v = parts[2 + k]
                    if v != "E" and int(v) == sz.cx:
                        right[k] += 1
                    elif len(miss[m]) < 6:
                        miss[m].append(f"{b:02x}:{v}/{sz.cx}")
            print(f"{path.name} {size}: " + ", ".join(f"{m} {r}/{n}" for m, r in zip(MODES, right)))
            print("    misses v35mono", miss["v35mono"], "linear", miss["linear"])
            gdi.SelectObject(dc, old)
            gdi.DeleteObject(hf)


if __name__ == "__main__":
    main()
