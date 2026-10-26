"""S5.1 probe 2: does GDI match a TrueType FULL name (name ID 4) as lfFaceName? Asked for faces of the host's font directory
before and after the same file is registered privately (AddFontResourceExA FR_PRIVATE), and for the game's manu3_.ttf.
    ~/bin/py -3.12 log/roadmap/S5/1_gdi_probe2.py"""
import ctypes, hashlib, os, pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import importlib.util
spec = importlib.util.spec_from_file_location("p1", pathlib.Path(__file__).with_name("1_gdi_probe.py"))
p1 = importlib.util.module_from_spec(spec); spec.loader.exec_module(p1)
gdi, LOGFONTA, TEXTMETRICA = p1.gdi, p1.LOGFONTA, p1.TEXTMETRICA
fontdir = pathlib.Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts"
files = {hashlib.sha256(p.read_bytes()).hexdigest(): p.name for p in fontdir.glob("*.tt[fc]")}
files[hashlib.sha256((p1.INSTALL / "manu3_.ttf").read_bytes()).hexdigest()] = "manu3_.ttf"
dc = gdi.CreateCompatibleDC(None)
def ask(face, weight=0, italic=0, h=-20):
    lf = LOGFONTA(); lf.lfHeight = h; lf.lfWeight = weight; lf.lfItalic = italic; lf.lfOutPrecision = 7
    lf.lfFaceName = face.encode()
    f = gdi.CreateFontIndirectA(ctypes.byref(lf)); old = gdi.SelectObject(dc, f)
    n = gdi.GetFontData(dc, 0, 0, None, 0); b = ctypes.create_string_buffer(n); gdi.GetFontData(dc, 0, 0, b, n)
    tb = ctypes.create_string_buffer(64); gdi.GetTextFaceA(dc, 64, tb)
    tm = TEXTMETRICA(); gdi.GetTextMetricsA(dc, ctypes.byref(tm))
    gdi.SelectObject(dc, old); gdi.DeleteObject(f)
    return f"{face!r:28} w{weight} i{italic} -> {files.get(hashlib.sha256(b.raw).hexdigest(), '?'):14} face {tb.value.decode()!r} tmWeight {tm.tmWeight} tmItalic {tm.tmItalic}"
for q in [("Arial Bold",), ("Arial",), ("Arial", 700), ("Arial", 0, 1), ("Times New Roman Bold",), ("Comic Sans MS Bold",),
          ("Comic Sans MS",), ("Comic Sans MS", 0, 1), ("Manual SSi Bold",), ("Manual SSi",), ("Manual SSi", 400), ("Nonexistent Face",),
          ("Nonexistent Face", 0, 1), ("Nonexistent Face", 700), ("arial bold",), ("MANUAL SSI BOLD",), ("Courier New Bold",)]:
    print("before", ask(*q))
print("register arialbd.ttf privately:", gdi.AddFontResourceExA(str(fontdir / "arialbd.ttf").encode(), p1.FR_PRIVATE, None))
print("register manu3_.ttf privately:", gdi.AddFontResourceExA(str(p1.INSTALL / "manu3_.ttf").encode(), p1.FR_PRIVATE, None))
for q in [("Arial Bold",), ("Manual SSi Bold",), ("manual ssi bold",), ("Manual SSi",), ("Manual SSi", 400), ("Manual SSi", 0, 1)]:
    print("after ", ask(*q))
# single-face system families whose full name differs from the family (1_fontdir_names.py)
for q in [("Unispace-Bold",), ("Unispace",), ("Cascadia Code Regular",), ("Cascadia Code",)]:
    print("single", ask(*q))
print("register unispace bd.ttf privately:", gdi.AddFontResourceExA(str(fontdir / "unispace bd.ttf").encode(), p1.FR_PRIVATE, None))
for q in [("Unispace-Bold",)]:
    print("single after", ask(*q))
