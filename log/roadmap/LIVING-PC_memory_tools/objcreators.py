"""Count live kernel objects of one type by the process that CREATED them (token-leak holder search, 2026-11-01).

Needs FLG_MAINTAIN_OBJECT_TYPELIST (GlobalFlag 0x4000 in HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager, set
2026-11-01 17:25) active since boot, and an ELEVATED prompt (SystemObjectInformation, class 17). Remove the flag afterwards.

Usage: ~/bin/py -3.12 objcreators.py [Type] [--pidlog pidlog.tsv] [--top 25]
  Type      object type name (default Token)
  --pidlog  a file written by pidlog.py since boot (pid, create time, image): names creators that have exited
Prints, per creator pid: live objects, how many have no handle (kernel-referenced), the pid's image (live or from the log).
Read-only.
"""
import ctypes, ctypes.wintypes as W, sys, collections, argparse

nt = ctypes.windll.ntdll


class US(ctypes.Structure):
    _fields_ = [("Length", W.USHORT), ("MaximumLength", W.USHORT), ("Buffer", ctypes.c_void_p)]


def live_images():
    size = 1 << 22
    while True:
        buf = ctypes.create_string_buffer(size); ret = W.ULONG()
        st = nt.NtQuerySystemInformation(5, buf, size, ctypes.byref(ret)) & 0xffffffff
        if st == 0xC0000004:
            size *= 2; continue
        break
    base = ctypes.addressof(buf); off = 0; out = {}
    while True:
        nxt = ctypes.c_ulong.from_address(base + off).value
        name = US.from_address(base + off + 0x38)
        img = ctypes.wstring_at(name.Buffer, name.Length // 2) if name.Buffer else "System Idle"
        out[ctypes.c_size_t.from_address(base + off + 0x50).value] = img
        if not nxt:
            break
        off += nxt
    return out


def object_list():
    size = 1 << 26
    for _ in range(8):
        buf = ctypes.create_string_buffer(size); ret = W.ULONG()
        st = nt.NtQuerySystemInformation(17, buf, size, ctypes.byref(ret)) & 0xffffffff
        if st in (0xC0000004, 0x80000005):
            size = max(size * 2, ret.value + (1 << 20)); continue
        return st, buf
    return st, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("type", nargs="?", default="Token")
    ap.add_argument("--pidlog")
    ap.add_argument("--top", type=int, default=25)
    a = ap.parse_args()
    st, buf = object_list()
    if st != 0 or buf is None:
        print(f"NtQuerySystemInformation(SystemObjectInformation) status {st:#x}: the object type list needs GlobalFlag 0x4000 "
              "active since boot and an elevated prompt")
        return 1
    base = ctypes.addressof(buf)
    names = live_images()
    logged = {}
    if a.pidlog:
        for line in open(a.pidlog, encoding="utf-8"):
            p = line.rstrip("\n").split("\t")
            if len(p) >= 3 and p[0].isdigit():
                logged.setdefault(int(p[0]), set()).add(p[2])
    toff = 0; found = False
    per = collections.Counter(); nohandle = collections.Counter(); total = 0
    while True:
        tname = US.from_address(base + toff + 0x30)
        tn = ctypes.wstring_at(tname.Buffer, tname.Length // 2) if tname.Buffer else ""
        if tn == a.type:
            found = True
            ooff = toff + 0x40 + ((tname.MaximumLength + 7) & ~7)
            while True:
                o = base + ooff
                creator = ctypes.c_size_t.from_address(o + 0x10).value
                handles = ctypes.c_long.from_address(o + 0x20).value
                per[creator] += 1; total += 1
                if handles == 0:
                    nohandle[creator] += 1
                nxt = ctypes.c_ulong.from_address(o).value
                if not nxt:
                    break
                ooff = nxt
        nxt = ctypes.c_ulong.from_address(base + toff).value
        if not nxt:
            break
        toff = nxt
    if not found:
        print(f"type {a.type} not in the list"); return 1
    print(f"{a.type}: {total} live objects, {sum(nohandle.values())} without a handle, {len(per)} creator pids")
    for pid, n in per.most_common(a.top):
        img = names.get(pid) or ("exited: " + "/".join(sorted(logged.get(pid, {"?"}))))
        print(f"{pid:>7}  {n:>8}  no handle {nohandle[pid]:>8}  {img}")
    return 0


sys.exit(main())
