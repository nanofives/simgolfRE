"""Read-only memory probe: GetPerformanceInfo + handle counts by object type per process (NtQuerySystemInformation 64)."""
import ctypes, ctypes.wintypes as W, collections, sys

class PERF(ctypes.Structure):
    _fields_ = [("cb", W.DWORD)] + [(n, ctypes.c_size_t) for n in (
        "CommitTotal", "CommitLimit", "CommitPeak", "PhysicalTotal", "PhysicalAvailable", "SystemCache",
        "KernelTotal", "KernelPaged", "KernelNonpaged", "PageSize")] + [("HandleCount", W.DWORD), ("ProcessCount", W.DWORD), ("ThreadCount", W.DWORD)]
p = PERF(); p.cb = ctypes.sizeof(p)
ctypes.windll.psapi.GetPerformanceInfo(ctypes.byref(p), p.cb)
pg = p.PageSize
for n in ("CommitTotal", "CommitLimit", "CommitPeak", "PhysicalTotal", "PhysicalAvailable", "SystemCache", "KernelTotal", "KernelPaged", "KernelNonpaged"):
    print(f"{n:18} {getattr(p, n) * pg / 2**30:8.2f} GB")
print("handles", p.HandleCount, "processes", p.ProcessCount, "threads", p.ThreadCount)

class ENTRY(ctypes.Structure):
    _fields_ = [("Object", ctypes.c_void_p), ("UniqueProcessId", ctypes.c_size_t), ("HandleValue", ctypes.c_size_t),
                ("GrantedAccess", W.ULONG), ("CreatorBackTraceIndex", W.USHORT), ("ObjectTypeIndex", W.USHORT),
                ("HandleAttributes", W.ULONG), ("Reserved", W.ULONG)]
nt = ctypes.windll.ntdll
size = 1 << 24
while True:
    buf = ctypes.create_string_buffer(size); ret = W.ULONG()
    st = nt.NtQuerySystemInformation(64, buf, size, ctypes.byref(ret)) & 0xffffffff
    if st == 0xC0000004: size *= 2; continue
    break
print("NtQuerySystemInformation status", hex(st))
if st: sys.exit()
count = ctypes.c_size_t.from_buffer(buf, 0).value
arr = (ENTRY * count).from_buffer(buf, 16)
per = collections.Counter(); bytype = collections.defaultdict(collections.Counter); objs = collections.defaultdict(set)
for e in arr:
    per[e.UniqueProcessId] += 1
    bytype[e.UniqueProcessId][e.ObjectTypeIndex] += 1
want = [int(x) for x in sys.argv[1:]]
for pid, n in per.most_common(8):
    print(pid, n, bytype[pid].most_common(5))
# which type index is Process? our own handle to our own process
k32 = ctypes.windll.kernel32
k32.OpenProcess.restype = W.HANDLE
import os
h = k32.OpenProcess(0x1000, False, os.getpid())
size = 1 << 24
while True:
    buf = ctypes.create_string_buffer(size); ret = W.ULONG()
    st = nt.NtQuerySystemInformation(64, buf, size, ctypes.byref(ret)) & 0xffffffff
    if st == 0xC0000004: size *= 2; continue
    break
count = ctypes.c_size_t.from_buffer(buf, 0).value
arr = (ENTRY * count).from_buffer(buf, 16)
for e in arr:
    if e.UniqueProcessId == os.getpid() and e.HandleValue == h:
        print("Process object type index =", e.ObjectTypeIndex)
