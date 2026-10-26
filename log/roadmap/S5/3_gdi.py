"""S5.3 GDI probe: TextOutA (BkMode TRANSPARENT, TA_BASELINE) on a 16- or 32-bit BI_RGB top-down DIB section, any
background, the game's font files added with FR_PRIVATE (this process only). Nothing system-wide is changed."""
import ctypes as c
import pathlib
from ctypes import wintypes as W

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[3]
FONT_FILES = [ROOT / "original" / "KLEPTO__.TTF", ROOT / "original" / "manu3_.TTF"]

FONTS = {   # the corpus FontRefs: face, lfHeight (recordings, labels.recorded_logfonts)
    0x51a028: ("Klepto ITC TT", -18), 0x51b360: ("Manual SSi Bold", -15), 0x519928: ("Manual SSi Bold", -20),
    0x519fd8: ("Arial Bold", -10), 0x519a40: ("Klepto ITC TT", -24), 0x821ee8: ("Manual SSi", -14),
    0x821020: ("Klepto ITC TT", -24), 0x821f28: ("Manual SSi", -16), 0x519948: ("Manual SSi Bold", -24),
}


class LOGFONTA(c.Structure):
    _fields_ = [("h", c.c_long), ("w", c.c_long), ("esc", c.c_long), ("ori", c.c_long),
                ("weight", c.c_long), ("italic", c.c_ubyte), ("under", c.c_ubyte), ("strike", c.c_ubyte),
                ("charset", c.c_ubyte), ("outp", c.c_ubyte), ("clip", c.c_ubyte), ("quality", c.c_ubyte),
                ("pitch", c.c_ubyte), ("face", c.c_char * 32)]


class BMI(c.Structure):
    _fields_ = [("size", c.c_uint32), ("w", c.c_int32), ("h", c.c_int32), ("planes", c.c_uint16),
                ("bpp", c.c_uint16), ("comp", c.c_uint32), ("isize", c.c_uint32), ("xp", c.c_int32),
                ("yp", c.c_int32), ("used", c.c_uint32), ("imp", c.c_uint32), ("pal", c.c_uint32 * 3)]


class Gdi:
    def __init__(self):
        g = self.g = c.windll.gdi32
        g.AddFontResourceExW.argtypes = [W.LPCWSTR, W.DWORD, c.c_void_p]
        for fn in FONT_FILES:
            if g.AddFontResourceExW(str(fn), 0x10, None) <= 0:
                raise RuntimeError(fn)
        g.CreateCompatibleDC.restype = W.HANDLE
        g.CreateCompatibleDC.argtypes = [W.HANDLE]
        g.CreateFontIndirectA.restype = W.HANDLE
        g.CreateDIBSection.restype = W.HANDLE
        g.CreateDIBSection.argtypes = [W.HANDLE, c.c_void_p, c.c_uint, c.POINTER(c.c_void_p), W.HANDLE, W.DWORD]
        g.SelectObject.restype = W.HANDLE
        g.SelectObject.argtypes = [W.HANDLE, W.HANDLE]
        g.DeleteObject.argtypes = [W.HANDLE]
        g.TextOutA.argtypes = [W.HANDLE, c.c_int, c.c_int, c.c_char_p, c.c_int]
        g.SetTextColor.argtypes = [W.HANDLE, W.DWORD]
        g.SetBkMode.argtypes = [W.HANDLE, c.c_int]
        g.SetTextAlign.argtypes = [W.HANDLE, c.c_uint]
        g.GetTextMetricsA.argtypes = [W.HANDLE, c.c_void_p]
        g.GetCharABCWidthsA.argtypes = [W.HANDLE, c.c_uint, c.c_uint, c.c_void_p]
        g.GetTextExtentPoint32A.argtypes = [W.HANDLE, c.c_char_p, c.c_int, c.c_void_p]
        self.dc = g.CreateCompatibleDC(None)
        self.fonts = {}

    def font(self, face, height, weight=0, italic=0, quality=0):
        key = (face, height, weight, italic, quality)
        if key not in self.fonts:
            lf = LOGFONTA(height, 0, 0, 0, weight, italic, 0, 0, 0, 7, 0, quality, 0, face.encode("latin-1"))
            self.fonts[key] = self.g.CreateFontIndirectA(c.byref(lf))
        return self.fonts[key]

    def draw(self, font, text: bytes, x, y, cref, bg, w, h, bpp=16):
        """bg: an array (h, w) of words (16 bpp) / 0x00RRGGBB dwords (32 bpp), or a scalar"""
        g = self.g
        bmi = BMI(40, w, -h, 1, bpp, 0)
        bits = c.c_void_p()
        bmp = g.CreateDIBSection(self.dc, c.byref(bmi), 0, c.byref(bits), None, 0)
        old = g.SelectObject(self.dc, bmp)
        if bpp == 16:
            pitch = (w + 1) & ~1
            arr = np.ctypeslib.as_array((c.c_uint16 * (pitch * h)).from_address(bits.value)).reshape(h, pitch)
        else:
            pitch = w
            arr = np.ctypeslib.as_array((c.c_uint32 * (pitch * h)).from_address(bits.value)).reshape(h, pitch)
        arr[:, :w] = bg
        g.SetBkMode(self.dc, 1)
        g.SetTextAlign(self.dc, 0x18)
        oldf = g.SelectObject(self.dc, font)
        g.SetTextColor(self.dc, cref)
        g.TextOutA(self.dc, x, y, text, len(text))
        g.GdiFlush()
        out = arr[:, :w].copy()
        g.SelectObject(self.dc, oldf)
        g.SelectObject(self.dc, old)
        g.DeleteObject(bmp)
        return out

    def abc(self, font, ch):
        a = (c.c_int * 3)()
        old = self.g.SelectObject(self.dc, font)
        self.g.GetCharABCWidthsA(self.dc, ch, ch, a)
        self.g.SelectObject(self.dc, old)
        return tuple(a)

    def extent(self, font, text: bytes):
        s = (c.c_long * 2)()
        old = self.g.SelectObject(self.dc, font)
        self.g.GetTextExtentPoint32A(self.dc, text, len(text), s)
        self.g.SelectObject(self.dc, old)
        return tuple(s)


def chan32(a):
    """(h, w) 0x00RRGGBB -> (h, w, 3) r g b"""
    return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], -1).astype(np.int32)


def chan16(a):
    """(h, w) RGB555 -> (h, w, 3) 5-bit r g b"""
    return np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], -1).astype(np.int32)
