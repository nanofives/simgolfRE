"""Log every process seen (pid, create time, image, parent pid) so objcreators.py can name creators that have exited.
Usage: ~/bin/py -3.12 pidlog.py <out.tsv> [interval_s=2]   (runs until killed; read-only)"""
import ctypes, ctypes.wintypes as W, sys, time

nt = ctypes.windll.ntdll


class US(ctypes.Structure):
    _fields_ = [("Length", W.USHORT), ("MaximumLength", W.USHORT), ("Buffer", ctypes.c_void_p)]


def procs():
    size = 1 << 22
    while True:
        buf = ctypes.create_string_buffer(size); ret = W.ULONG()
        st = nt.NtQuerySystemInformation(5, buf, size, ctypes.byref(ret)) & 0xffffffff
        if st == 0xC0000004:
            size *= 2; continue
        break
    base = ctypes.addressof(buf); off = 0
    while True:
        nxt = ctypes.c_ulong.from_address(base + off).value
        create = ctypes.c_longlong.from_address(base + off + 0x20).value
        name = US.from_address(base + off + 0x38)
        img = ctypes.wstring_at(name.Buffer, name.Length // 2) if name.Buffer else "System Idle"
        pid = ctypes.c_size_t.from_address(base + off + 0x50).value
        ppid = ctypes.c_size_t.from_address(base + off + 0x58).value
        yield pid, create, img, ppid
        if not nxt:
            break
        off += nxt


out = sys.argv[1]; dt = float(sys.argv[2]) if len(sys.argv) > 2 else 2.0
seen = set()
with open(out, "a", encoding="utf-8") as f:
    while True:
        for pid, create, img, ppid in procs():
            if (pid, create) not in seen:
                seen.add((pid, create))
                f.write(f"{pid}\t{create}\t{img}\t{ppid}\t{time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.flush()
        time.sleep(dt)
