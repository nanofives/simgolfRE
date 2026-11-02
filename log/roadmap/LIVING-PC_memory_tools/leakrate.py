"""Token / Section object counts before and after spawning N trivial processes, and over an idle minute."""
import ctypes, ctypes.wintypes as W, subprocess, time, sys
nt = ctypes.windll.ntdll
class US(ctypes.Structure):
    _fields_ = [("Length", W.USHORT), ("MaximumLength", W.USHORT), ("Buffer", ctypes.c_void_p)]
def counts():
    size = 1 << 20; buf = ctypes.create_string_buffer(size); ret = W.ULONG()
    nt.NtQueryObject(None, 3, buf, size, ctypes.byref(ret))
    n = ctypes.c_ulong.from_buffer(buf, 0).value; off = 8; base = ctypes.addressof(buf); out = {}
    for i in range(n):
        name = US.from_address(base + off); s = ctypes.wstring_at(name.Buffer, name.Length // 2)
        p = base + off + 16
        out[s] = (ctypes.c_ulong.from_address(p).value, ctypes.c_ulong.from_address(p + 4).value)
        off += 16 + 4 * 13 + 16 + 4 + 4 + 4 + 4 + 4; off += (name.MaximumLength + 7) & ~7
    return out
def show(tag, a, b):
    print(tag, {k: (b[k][0] - a[k][0], b[k][1] - a[k][1]) for k in ("Token", "Section", "Process", "Thread")})
a = counts(); time.sleep(60); b = counts(); show("idle 60 s (objects, handles) delta:", a, b)
N = int(sys.argv[1]) if len(sys.argv) > 1 else 100
a = counts()
for _ in range(N):
    subprocess.run(["cmd.exe", "/c", "exit 0"], creationflags=0x08000000)
time.sleep(5); b = counts(); show(f"after {N} cmd.exe spawns:", a, b)
print("now Token", b["Token"], "Section", b["Section"])
