"""S5.1 probe: which font FILE the host GDI selects for each of the game's 15 FontRefs (Font::create's LOGFONTA, as
gditext.cpp builds it), its TEXTMETRIC words and which of the tables GDI's sizing may use (VDMX, hdmx, LTSH, kern, EBLC)
the file has. The game's two font files are registered privately (AddFontResourceExA FR_PRIVATE) from the install
(read only). The selected file is identified by hashing GetFontData(dc, 0, 0) (the whole file as GDI holds it) against
every file in the host's font directory and the two game files.

    ~/bin/py -3.12 log/roadmap/S5/1_gdi_probe.py [--json out.json]
"""
import argparse
import ctypes
import ctypes.wintypes as W
import hashlib
import json
import os
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parents[3]
INSTALL = ROOT / "original"
gdi = ctypes.WinDLL("gdi32")
FR_PRIVATE = 0x10


class LOGFONTA(ctypes.Structure):
    _fields_ = [("lfHeight", W.LONG), ("lfWidth", W.LONG), ("lfEscapement", W.LONG), ("lfOrientation", W.LONG),
                ("lfWeight", W.LONG), ("lfItalic", W.BYTE), ("lfUnderline", W.BYTE), ("lfStrikeOut", W.BYTE),
                ("lfCharSet", W.BYTE), ("lfOutPrecision", W.BYTE), ("lfClipPrecision", W.BYTE),
                ("lfQuality", W.BYTE), ("lfPitchAndFamily", W.BYTE), ("lfFaceName", ctypes.c_char * 32)]


class TEXTMETRICA(ctypes.Structure):
    _fields_ = [(n, W.LONG) for n in ("tmHeight", "tmAscent", "tmDescent", "tmInternalLeading", "tmExternalLeading",
                                      "tmAveCharWidth", "tmMaxCharWidth", "tmWeight", "tmOverhang",
                                      "tmDigitizedAspectX", "tmDigitizedAspectY")] + \
               [(n, ctypes.c_char) for n in ("tmFirstChar", "tmLastChar", "tmDefaultChar", "tmBreakChar")] + \
               [(n, W.BYTE) for n in ("tmItalic", "tmUnderlined", "tmStruckOut", "tmPitchAndFamily", "tmCharSet")]


gdi.CreateFontIndirectA.restype = W.HFONT
gdi.CreateFontIndirectA.argtypes = [ctypes.POINTER(LOGFONTA)]
gdi.CreateCompatibleDC.restype = W.HDC
gdi.CreateCompatibleDC.argtypes = [W.HDC]
gdi.SelectObject.restype = W.HGDIOBJ
gdi.SelectObject.argtypes = [W.HDC, W.HGDIOBJ]
gdi.GetFontData.restype = W.DWORD
gdi.GetFontData.argtypes = [W.HDC, W.DWORD, W.DWORD, ctypes.c_void_p, W.DWORD]
gdi.GetTextFaceA.argtypes = [W.HDC, ctypes.c_int, ctypes.c_char_p]
gdi.GetTextMetricsA.argtypes = [W.HDC, ctypes.POINTER(TEXTMETRICA)]
gdi.AddFontResourceExA.argtypes = [ctypes.c_char_p, W.DWORD, ctypes.c_void_p]
gdi.DeleteObject.argtypes = [W.HGDIOBJ]

# (FontRef, face, size before scaling, scaled, style, file): gditext.cpp's kFonts with the faces the 38a notes list
FONTS = [("g_font83ad20", "Times New Roman", 12, False, 0, None),
         ("g_font51b320", "Arial Bold", 9, False, 0, None),
         ("g_fontRef_519fd8", "Arial Bold", 10, False, 0, None),
         ("g_font51b360", "Manual SSi Bold", 15, False, 0, "manu3_.ttf"),
         ("g_font519928", "Manual SSi Bold", 20, False, 0, "manu3_.ttf"),
         ("g_font51a028", "Klepto ITC TT", 18, False, 0, "klepto__.ttf"),
         ("g_fontRef_519a40", "Klepto ITC TT", 24, False, 0, "klepto__.ttf"),
         ("g_font519948", "Manual SSi Bold", 24, False, 0, "manu3_.ttf"),
         ("g_font51b340", "Comic Sans MS Bold", 40, True, 2, None),
         ("g_font519968", "Times New Roman Bold", 7, True, 0, None),
         ("g_fontRef_821020", "Klepto ITC TT", 24, False, 0, None),
         ("g_fontRef_821f08", "Manual SSi", 20, False, 0, None),
         ("g_fontRef_821ec8", "Manual SSi", 18, False, 0, None),
         ("g_fontRef_821f28", "Manual SSi", 16, False, 0, None),
         ("g_fontRef_821ee8", "Manual SSi", 14, False, 0, None)]
SCREEN_W = 800


def tag(s):
    return struct.unpack("<I", s.encode())[0]   # GDI wants the tag bytes in memory order (first char lowest)


def table(dc, t):
    n = gdi.GetFontData(dc, tag(t), 0, None, 0)
    if n in (0, 0xFFFFFFFF):
        return None
    b = ctypes.create_string_buffer(n)
    gdi.GetFontData(dc, tag(t), 0, b, n)
    return b.raw


def names(blob):
    out = {}
    if not blob:
        return out
    _, count, off = struct.unpack_from(">HHH", blob, 0)
    for i in range(count):
        pid, eid, lid, nid, ln, o = struct.unpack_from(">6H", blob, 6 + 12 * i)
        raw = blob[off + o: off + o + ln]
        s = raw.decode("utf-16-be", "replace") if pid in (0, 3) else raw.decode("latin-1")
        out.setdefault(nid, set()).add((pid, eid, lid, s))
    return {k: sorted(v) for k, v in out.items() if k in (1, 2, 4, 6, 16, 17)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json")
    a = ap.parse_args()
    files = {}
    fontdir = pathlib.Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts"
    for p in list(fontdir.glob("*.tt[fc]")) + [INSTALL / "manu3_.ttf", INSTALL / "klepto__.ttf"]:
        try:
            files.setdefault(hashlib.sha256(p.read_bytes()).hexdigest(), str(p))
        except OSError:
            pass
    for f in ("manu3_.ttf", "klepto__.ttf"):
        print("AddFontResourceExA", f, gdi.AddFontResourceExA(str(INSTALL / f).encode(), FR_PRIVATE, None))
    dc = gdi.CreateCompatibleDC(None)
    out = []
    for ref, face, size, scaled, style, file in FONTS:
        if scaled:
            size = (SCREEN_W * size + 160) // 320
        lf = LOGFONTA()
        lf.lfHeight = -size
        lf.lfWeight = 700 if style & 1 else 0
        lf.lfItalic = 1 if style & 2 else 0
        lf.lfUnderline = 1 if style & 4 else 0
        lf.lfOutPrecision = 7
        lf.lfFaceName = face.encode()
        h = gdi.CreateFontIndirectA(ctypes.byref(lf))
        old = gdi.SelectObject(dc, h)
        tm = TEXTMETRICA()
        gdi.GetTextMetricsA(dc, ctypes.byref(tm))
        fb = ctypes.create_string_buffer(64)
        gdi.GetTextFaceA(dc, 64, fb)
        n = gdi.GetFontData(dc, 0, 0, None, 0)
        whole = ctypes.create_string_buffer(n)
        gdi.GetFontData(dc, 0, 0, whole, n)
        sel = files.get(hashlib.sha256(whole.raw).hexdigest(), f"unknown ({n} bytes)")
        head = table(dc, "head")
        os2 = table(dc, "OS/2")
        row = {"fontref": ref, "face": face, "size": size, "style": style, "file_arg": file,
               "selected_file": sel, "text_face": fb.value.decode("latin-1"),
               "five": [tm.tmHeight + tm.tmExternalLeading, tm.tmAscent - tm.tmInternalLeading, tm.tmInternalLeading,
                        tm.tmAscent, tm.tmDescent],
               "tm": {k: getattr(tm, k) for k in ("tmHeight", "tmAscent", "tmDescent", "tmInternalLeading",
                                                   "tmExternalLeading", "tmAveCharWidth", "tmMaxCharWidth",
                                                   "tmWeight", "tmOverhang", "tmItalic")},
               "tables": {t: (len(table(dc, t)) if table(dc, t) else 0) for t in ("VDMX", "hdmx", "LTSH", "kern", "EBLC",
                                                                                   "fpgm", "prep", "gasp")},
               "head_flags": struct.unpack_from(">H", head, 16)[0] if head else None,
               "units_per_em": struct.unpack_from(">H", head, 18)[0] if head else None,
               "os2_weight": struct.unpack_from(">H", os2, 4)[0] if os2 else None,
               "os2_fsSelection": struct.unpack_from(">H", os2, 62)[0] if os2 else None,
               "names": names(table(dc, "name"))}
        out.append(row)
        print(json.dumps({k: v for k, v in row.items() if k != "names"}))
        gdi.SelectObject(dc, old)
        gdi.DeleteObject(h)
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps(out, indent=1, default=list))


if __name__ == "__main__":
    main()
