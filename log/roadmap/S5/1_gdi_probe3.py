"""S5.1 probe 3: GDI's synthetic bold and italic on TrueType faces that lack the style (Klepto ITC TT and Times New
Roman Bold Italic is real; Klepto has only Roman): per-character extents and the five FontRef words with and without
the synthesis, at the game's sizes.
    ~/bin/py -3.12 log/roadmap/S5/1_gdi_probe3.py"""
import ctypes, ctypes.wintypes as W, importlib.util, pathlib
spec = importlib.util.spec_from_file_location("p1", pathlib.Path(__file__).with_name("1_gdi_probe.py"))
p1 = importlib.util.module_from_spec(spec); spec.loader.exec_module(p1)
gdi = p1.gdi
gdi.GetTextExtentPoint32A.argtypes = [W.HDC, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(W.SIZE)]
for f in ("manu3_.ttf", "klepto__.ttf"):
    gdi.AddFontResourceExA(str(p1.INSTALL / f).encode(), p1.FR_PRIVATE, None)
dc = gdi.CreateCompatibleDC(None)
def font(face, size, weight, italic):
    lf = p1.LOGFONTA(); lf.lfHeight = -size; lf.lfWeight = weight; lf.lfItalic = italic; lf.lfOutPrecision = 7
    lf.lfFaceName = face.encode()
    return gdi.CreateFontIndirectA(ctypes.byref(lf))
def measure(face, size, weight, italic, texts):
    h = font(face, size, weight, italic); old = gdi.SelectObject(dc, h)
    tm = p1.TEXTMETRICA(); gdi.GetTextMetricsA(dc, ctypes.byref(tm))
    out = []
    for t in texts:
        sz = W.SIZE(); gdi.GetTextExtentPoint32A(dc, t, len(t), ctypes.byref(sz)); out.append(sz.cx)
    gdi.SelectObject(dc, old); gdi.DeleteObject(h)
    five = [tm.tmHeight + tm.tmExternalLeading, tm.tmAscent - tm.tmInternalLeading, tm.tmInternalLeading, tm.tmAscent, tm.tmDescent]
    return five, tm.tmOverhang, tm.tmWeight, tm.tmItalic, out
texts = [bytes([c]) for c in range(0x20, 0x7f)] + [b"Continue Saved Game", b"iiiii", b"WWWWW"]
for face, size in (("Klepto ITC TT", 18), ("Klepto ITC TT", 24), ("Manual SSi", 14), ("Times New Roman", 12)):
    base = measure(face, size, 0, 0, texts)
    for w, i in ((700, 0), (0, 1), (700, 1)):
        m = measure(face, size, w, i, texts)
        d = [b - a for a, b in zip(base[4], m[4])]
        print(f"{face} {size} w{w} i{i}: five {base[0]} -> {m[0]} overhang {base[1]} -> {m[1]} weight {m[2]} italic {m[3]};"
              f" extent deltas over {len(d)} texts: {sorted(set(d))}; per text {d[-3:]}")
