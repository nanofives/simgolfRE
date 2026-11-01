"""Print free commit charge (GB); exit 1 if below the threshold (default 16 GB). Used by W8_HEAVY_RULE.md."""
import ctypes, sys

class MS(ctypes.Structure):
    _fields_ = [("dwLength", ctypes.c_ulong), ("dwMemoryLoad", ctypes.c_ulong),
                ("ullTotalPhys", ctypes.c_ulonglong), ("ullAvailPhys", ctypes.c_ulonglong),
                ("ullTotalPageFile", ctypes.c_ulonglong), ("ullAvailPageFile", ctypes.c_ulonglong),
                ("ullTotalVirtual", ctypes.c_ulonglong), ("ullAvailVirtual", ctypes.c_ulonglong),
                ("ullAvailExtendedVirtual", ctypes.c_ulonglong)]

m = MS(); m.dwLength = ctypes.sizeof(MS)
ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(m))
gb = m.ullAvailPageFile / 2**30
need = float(sys.argv[1]) if len(sys.argv) > 1 else 16.0
print(f"free commit {gb:.1f} GB (need {need:g})")
sys.exit(0 if gb >= need else 1)
