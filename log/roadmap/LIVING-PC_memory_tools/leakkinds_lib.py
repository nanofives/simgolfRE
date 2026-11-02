"""Token objects kept per process launch, by kind of process, against an idle control of the same length.

Usage: ~/bin/py -3.12 leakkinds.py [N] [rounds]
For each round: idle control (sleep for as long as the previous kind's batch took), then N launches of each kind. Prints
the Token (objects, handles) delta of each batch and of its control, and per launch the difference. Read-only apart from the
trivial child processes it starts (each exits at once).
"""
import ctypes, ctypes.wintypes as W

nt = ctypes.windll.ntdll


class US(ctypes.Structure):
    _fields_ = [("Length", W.USHORT), ("MaximumLength", W.USHORT), ("Buffer", ctypes.c_void_p)]


def token_counts():
    size = 1 << 20; buf = ctypes.create_string_buffer(size); ret = W.ULONG()
    nt.NtQueryObject(None, 3, buf, size, ctypes.byref(ret))
    n = ctypes.c_ulong.from_buffer(buf, 0).value; off = 8; base = ctypes.addressof(buf)
    for _ in range(n):
        name = US.from_address(base + off); s = ctypes.wstring_at(name.Buffer, name.Length // 2)
        p = base + off + 16
        if s == "Token":
            return ctypes.c_ulong.from_address(p).value, ctypes.c_ulong.from_address(p + 4).value
        off += 16 + 4 * 13 + 16 + 4 + 4 + 4 + 4 + 4; off += (name.MaximumLength + 7) & ~7
    return -1, -1


