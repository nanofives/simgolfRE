"""Relate Token object growth to process launches by image name (read-only; no admin needed).

Usage: ~/bin/py -3.12 tokensampler.py <minutes> <out.tsv>
Every 10 s: Token object / handle counts (NtQueryObject ObjectTypesInformation) and the live processes
(NtQuerySystemInformation SystemProcessInformation: pid, create time, image name). A process is NEW in an interval when its
(pid, create time) was not seen before. Writes one row per interval (seconds, token delta, handle delta, new processes by name)
and, at the end, a least-squares fit token_delta = base * seconds + sum(k_name * launches_name) over the names with the most
launches. Rows are flushed as they come.
"""
import ctypes, ctypes.wintypes as W, time, sys, collections
import numpy as np

nt = ctypes.windll.ntdll


class US(ctypes.Structure):
    _fields_ = [("Length", W.USHORT), ("MaximumLength", W.USHORT), ("Buffer", ctypes.c_void_p)]


def tokens():
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


def processes():
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
        create = ctypes.c_longlong.from_address(base + off + 0x20).value
        name = US.from_address(base + off + 0x38)
        img = ctypes.wstring_at(name.Buffer, name.Length // 2) if name.Buffer else "System Idle"
        pid = ctypes.c_size_t.from_address(base + off + 0x50).value
        out[(pid, create)] = img.lower()
        if not nxt:
            break
        off += nxt
    return out


minutes = float(sys.argv[1]); path = sys.argv[2]
seen = set(processes()); t_prev = time.time(); k_prev = tokens()
rows = []
with open(path, "w", encoding="utf-8") as f:
    f.write("t\tseconds\ttoken_delta\thandle_delta\tnew\n")
    end = time.time() + minutes * 60
    while time.time() < end:
        time.sleep(10)
        procs = processes(); k = tokens(); t = time.time()
        new = collections.Counter(img for key, img in procs.items() if key not in seen)
        seen.update(procs)
        row = (t - t_prev, k[0] - k_prev[0], k[1] - k_prev[1], new)
        rows.append(row)
        f.write(f"{time.strftime('%H:%M:%S')}\t{row[0]:.1f}\t{row[1]}\t{row[2]}\t"
                + ",".join(f"{n}:{c}" for n, c in new.most_common()) + "\n")
        f.flush()
        t_prev, k_prev = t, k
    total = collections.Counter()
    for r in rows:
        total.update(r[3])
    names = [n for n, _ in total.most_common(15)]
    X = np.array([[r[0]] + [r[3].get(n, 0) for n in names] for r in rows], dtype=float)
    y = np.array([r[1] for r in rows], dtype=float)
    coef, *_ = np.linalg.lstsq(X, y, rcond=None)
    f.write(f"# intervals {len(rows)} token growth {int(y.sum())} in {X[:, 0].sum():.0f} s\n")
    f.write(f"# fit base {coef[0]:+.2f} tokens/s\n")
    for n, c in zip(names, coef[1:]):
        f.write(f"# fit {n}\tlaunches {total[n]}\ttokens per launch {c:+.2f}\n")
print(open(path, encoding="utf-8").read().split("# intervals")[-1])
