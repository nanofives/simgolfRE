"""Delta of every kernel object type (objects, handles) over an interval. Usage: ~/bin/py -3.12 objdelta.py <seconds>"""
import ctypes, ctypes.wintypes as W, time, sys
nt = ctypes.windll.ntdll
class US(ctypes.Structure): _fields_ = [("Length", W.USHORT), ("MaximumLength", W.USHORT), ("Buffer", ctypes.c_void_p)]
def counts():
    size = 1 << 20; buf = ctypes.create_string_buffer(size); ret = W.ULONG()
    nt.NtQueryObject(None, 3, buf, size, ctypes.byref(ret))
    n = ctypes.c_ulong.from_buffer(buf, 0).value; off = 8; base = ctypes.addressof(buf); out = {}
    for _ in range(n):
        name = US.from_address(base + off); s = ctypes.wstring_at(name.Buffer, name.Length // 2); p = base + off + 16
        out[s] = (ctypes.c_ulong.from_address(p).value, ctypes.c_ulong.from_address(p + 4).value)
        off += 16 + 4 * 13 + 16 + 4 + 4 + 4 + 4 + 4; off += (name.MaximumLength + 7) & ~7
    return out
a = counts(); time.sleep(float(sys.argv[1])); b = counts()
rows = sorted(((b[k][0] - a[k][0], b[k][1] - a[k][1], k, b[k][0]) for k in b if k in a), reverse=True)
for d, h, k, tot in rows[:12]: print(f"{k:28} objects {d:+7d} handles {h:+7d} now {tot}")
print("...")
for d, h, k, tot in rows[-4:]: print(f"{k:28} objects {d:+7d} handles {h:+7d} now {tot}")
