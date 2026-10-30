"""S3.20 band measurement: where a spinning thread is. Samples a thread's RIP n times (SuspendThread / GetThreadContext / ResumeThread) and
names the module and offset of each sample (EnumProcessModulesEx + GetModuleInformation).

    ~/bin/py -3.12 log/roadmap/S3/20_band/thread_rip.py <pid> <tid> [n]
"""
import collections
import ctypes as C
import ctypes.wintypes as W
import sys
import time

k32 = C.WinDLL("kernel32", use_last_error=True)
psapi = C.WinDLL("psapi", use_last_error=True)


class CONTEXT64(C.Structure):
    _pack_ = 16
    _fields_ = [("P1Home", C.c_uint64), ("P2Home", C.c_uint64), ("P3Home", C.c_uint64), ("P4Home", C.c_uint64), ("P5Home", C.c_uint64),
                ("P6Home", C.c_uint64), ("ContextFlags", W.DWORD), ("MxCsr", W.DWORD), ("SegCs", W.WORD), ("SegDs", W.WORD),
                ("SegEs", W.WORD), ("SegFs", W.WORD), ("SegGs", W.WORD), ("SegSs", W.WORD), ("EFlags", W.DWORD),
                ("Dr0", C.c_uint64), ("Dr1", C.c_uint64), ("Dr2", C.c_uint64), ("Dr3", C.c_uint64), ("Dr6", C.c_uint64), ("Dr7", C.c_uint64),
                ("Rax", C.c_uint64), ("Rcx", C.c_uint64), ("Rdx", C.c_uint64), ("Rbx", C.c_uint64), ("Rsp", C.c_uint64), ("Rbp", C.c_uint64),
                ("Rsi", C.c_uint64), ("Rdi", C.c_uint64), ("R8", C.c_uint64), ("R9", C.c_uint64), ("R10", C.c_uint64), ("R11", C.c_uint64),
                ("R12", C.c_uint64), ("R13", C.c_uint64), ("R14", C.c_uint64), ("R15", C.c_uint64), ("Rip", C.c_uint64),
                ("rest", C.c_byte * 976)]


class MODINFO(C.Structure):
    _fields_ = [("base", C.c_void_p), ("size", W.DWORD), ("entry", C.c_void_p)]


def modules(hp):
    arr = (C.c_void_p * 1024)()
    need = W.DWORD()
    psapi.EnumProcessModulesEx(hp, arr, C.sizeof(arr), C.byref(need), 3)
    out = []
    for i in range(need.value // C.sizeof(C.c_void_p)):
        name = C.create_unicode_buffer(260)
        psapi.GetModuleBaseNameW(hp, C.c_void_p(arr[i]), name, 260)
        mi = MODINFO()
        psapi.GetModuleInformation(hp, C.c_void_p(arr[i]), C.byref(mi), C.sizeof(mi))
        out.append((mi.base or 0, mi.size, name.value))
    return out


def main():
    pid, tid = int(sys.argv[1]), int(sys.argv[2])
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 20
    hp = k32.OpenProcess(0x0410, False, pid)
    ht = k32.OpenThread(0x0002 | 0x0008 | 0x0040, False, tid)   # SUSPEND_RESUME | GET_CONTEXT | QUERY_INFORMATION
    if not hp or not ht:
        sys.exit(f"open failed {C.get_last_error()}")
    mods = modules(hp)
    hits = collections.Counter()
    for _ in range(n):
        ctx = CONTEXT64()
        ctx.ContextFlags = 0x00100001   # CONTEXT_AMD64 | CONTEXT_CONTROL
        k32.SuspendThread(ht)
        ok = k32.GetThreadContext(ht, C.byref(ctx))
        k32.ResumeThread(ht)
        if not ok:
            hits["?"] += 1
            continue
        rip = ctx.Rip
        name = next((f"{m}+0x{rip - b:x}" for b, s, m in mods if b <= rip < b + s), f"0x{rip:x}")
        hits[name] += 1
        time.sleep(0.05)
    for k, v in hits.most_common():
        print(v, k)


if __name__ == "__main__":
    main()
