"""Token objects kept per process launch, by kind of process, against an idle control of the same length.

Usage: ~/bin/py -3.12 leakkinds.py [N] [rounds]
For each round: idle control (sleep for as long as the previous kind's batch took), then N launches of each kind. Prints
the Token (objects, handles) delta of each batch and of its control, and per launch the difference. Read-only apart from the
trivial child processes it starts (each exits at once).
"""
import ctypes, ctypes.wintypes as W, subprocess, time, sys, shutil

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


NOWIN = 0x08000000
bash = shutil.which("bash") or r"C:\Program Files\Git\bin\bash.exe"
node = shutil.which("node")
KINDS = {
    "cmd": ["cmd.exe", "/c", "exit 0"],
    "python": [sys.executable, "-c", "pass"],
    "powershell": ["powershell.exe", "-NoProfile", "-Command", "exit 0"],
    "bash": [bash, "-c", "true"],
    "git": ["git", "--version"],
}
if node:
    KINDS["node"] = [node, "-e", "0"]

N = int(sys.argv[1]) if len(sys.argv) > 1 else 30
ROUNDS = int(sys.argv[2]) if len(sys.argv) > 2 else 2
print(f"N={N} rounds={ROUNDS} start Token {token_counts()}")
for r in range(ROUNDS):
    for kind, cmd in KINDS.items():
        a = token_counts(); t0 = time.time()
        for _ in range(N):
            subprocess.run(cmd, creationflags=NOWIN, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(2)
        b = token_counts(); dt = time.time() - t0
        c0 = token_counts(); time.sleep(dt); c1 = token_counts()
        batch = b[0] - a[0]; idle = c1[0] - c0[0]
        print(f"round {r} {kind:10} batch {dt:5.1f} s tokens {batch:+6d} (handles {b[1] - a[1]:+d})  idle control {idle:+6d}"
              f"  per launch {(batch - idle) / N:+.2f}", flush=True)
print("end Token", token_counts())
