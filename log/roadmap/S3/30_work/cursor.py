"""Park / restore the host's mouse cursor (U-1970): a WM_MOUSEMOVE the system sends because the physical cursor rests over the game
window is host input no harness DELIVER explains. park: move it to a taskbar point (WindowFromPoint class checked) and save the old
point; restore: move it back."""
import ctypes, json, pathlib, sys
U = ctypes.windll.user32
class P(ctypes.Structure): _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]
SAVE = pathlib.Path(__file__).with_name("cursor_saved.json")
def cls_at(x, y):
    h = U.WindowFromPoint(P(x, y)); b = ctypes.create_unicode_buffer(256)
    U.GetClassNameW(U.GetAncestor(h, 2), b, 256); return b.value
cmd = sys.argv[1]
if cmd == "park":
    p = P(); U.GetCursorPos(ctypes.byref(p))
    if not SAVE.exists(): SAVE.write_text(json.dumps([p.x, p.y]))
    w, h = U.GetSystemMetrics(0), U.GetSystemMetrics(1)
    for x, y in [(w - 4, h - 4), (w // 2, h - 4), (4, h - 4)]:
        c = cls_at(x, y)
        print("candidate", x, y, c)
        if c in ("Shell_TrayWnd", "Shell_SecondaryTrayWnd"):
            U.SetCursorPos(x, y); print("parked at", x, y, "saved", SAVE.read_text()); break
    else:
        sys.exit("no taskbar point found")
elif cmd == "restore":
    x, y = json.loads(SAVE.read_text()); U.SetCursorPos(x, y); SAVE.unlink(); print("restored", x, y)
else:
    p = P(); U.GetCursorPos(ctypes.byref(p)); print("cursor", p.x, p.y, cls_at(p.x, p.y))
