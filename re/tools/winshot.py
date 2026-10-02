"""Screenshot the game window (client area) of a given PID. Usage: py -3.12 re/tools/winshot.py <pid> <out.png>"""
import ctypes, ctypes.wintypes as wt, sys
from PIL import ImageGrab
user32 = ctypes.windll.user32
user32.SetProcessDPIAware()

def find_windows(pid, cls=None):
    out = []
    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(h, _):
        p = wt.DWORD(); user32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(h):
            buf = ctypes.create_unicode_buffer(256); user32.GetClassNameW(h, buf, 256)
            if cls is None or buf.value == cls:
                out.append((h, buf.value))
        return True
    user32.EnumWindows(cb, 0)
    return out

def client_rect(h):
    rc = wt.RECT(); user32.GetClientRect(h, ctypes.byref(rc))
    pt = wt.POINT(0, 0); user32.ClientToScreen(h, ctypes.byref(pt))
    return pt.x, pt.y, pt.x + rc.right, pt.y + rc.bottom

def shot(pid, out, cls="JackalClass"):
    wins = find_windows(pid, cls)
    if not wins:
        raise SystemExit(f"no visible {cls} window for pid {pid}")
    box = client_rect(wins[0][0])
    ImageGrab.grab(bbox=box, all_screens=True).save(out)
    return box

if __name__ == "__main__":
    print(shot(int(sys.argv[1]), sys.argv[2]))
