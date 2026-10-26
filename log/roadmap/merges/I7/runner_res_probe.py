"""I7: resources of ONE long-lived Win32 runner across live equivalence runs (test_live.py::test_equivalence_all's loop), to find
what accumulates there: after each scenario print the runner's GDI objects, USER objects, handle count and private bytes.
Run from a mirror root: SG_BACKEND=win32 SG_PORTSIM_W32_DIR=<root>/port/build/Win32 ~/bin/py -3.12 <this> [n scenarios] [--reps k]"""
import ctypes
import os
import sys
import time

sys.path.insert(0, os.getcwd())
from port.test import lib as L, live as LV, boot as BOOT  # noqa: E402

k32, u32 = ctypes.windll.kernel32, ctypes.windll.user32


class PMC(ctypes.Structure):
    _fields_ = [("cb", ctypes.c_ulong), ("PageFaultCount", ctypes.c_ulong), ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t), ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t), ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t), ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t), ("PrivateUsage", ctypes.c_size_t)]


def res(pid):
    h = k32.OpenProcess(0x0410 | 0x1000, False, pid)
    if not h:
        return "gone"
    gdi, usr = u32.GetGuiResources(h, 0), u32.GetGuiResources(h, 1)
    hc = ctypes.c_ulong()
    k32.GetProcessHandleCount(h, ctypes.byref(hc))
    pmc = PMC()
    pmc.cb = ctypes.sizeof(PMC)
    ctypes.windll.psapi.GetProcessMemoryInfo(h, ctypes.byref(pmc), pmc.cb)
    k32.CloseHandle(h)
    return f"gdi {gdi} user {usr} handles {hc.value} private {pmc.PrivateUsage // 2**20} MB"


def main():
    a = sys.argv[1:]
    n = int(a[0]) if a and a[0].isdigit() else 12
    reps = int(a[a.index("--reps") + 1]) if "--reps" in a else 1
    lib = L.load()
    names = [x for x in a if not x.isdigit() and not x.startswith("--") and x not in (str(reps),)] or BOOT.scenarios()[:n]
    pid = lib.runner.proc.pid
    print("runner pid", pid, "start", res(pid), flush=True)
    for r in range(reps):
        for s in names:
            t0 = time.time()
            try:
                e = LV.equivalence(s, lib)
                st = e.status if e.status in ("PASS", "NO_FRAMES") else f"{e.status} {e.detail[:120]} {str(e.extra.get('live_detail', ''))[:160]} live_code {e.extra.get('live_code')}"
            except Exception as ex:  # noqa: BLE001
                st = f"EXC {type(ex).__name__}: {str(ex)[:80]}"
            print(f"rep {r} {s:24} {st:10} {time.time() - t0:6.1f} s  {res(pid)}", flush=True)


if __name__ == "__main__":
    main()
