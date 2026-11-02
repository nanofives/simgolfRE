"""Token objects kept per child launched FROM INSIDE an MSYS bash (Git Bash) loop, against idle controls of equal length.
Usage: ~/bin/py -3.12 leakmsys.py [N] [rounds]. Each kind runs one bash that launches N children; the per-child figure
subtracts the idle control and the one bash launch (measured as kind `bash-only`)."""
import subprocess, time, sys, shutil
sys.path.insert(0, __file__.rsplit("\\", 1)[0].rsplit("/", 1)[0])
from leakkinds_lib import token_counts
bash = shutil.which("bash") or r"C:\Program Files\Git\bin\bash.exe"
N = int(sys.argv[1]) if len(sys.argv) > 1 else 50
R = int(sys.argv[2]) if len(sys.argv) > 2 else 2
KINDS = {
    "bash-only": "true",
    "msys sleep 0": f"for i in $(seq {N}); do sleep 0; done",
    "msys true.exe": f"for i in $(seq {N}); do /usr/bin/true; done",
    "native cmd": f"for i in $(seq {N}); do cmd.exe //c exit 0; done",
    "native whoami": f"for i in $(seq {N}); do /c/Windows/System32/hostname.exe >/dev/null; done",
    "subshell": f"for i in $(seq {N}); do ( : ); done",
}
print(f"N={N} rounds={R} start Token {token_counts()}", flush=True)
for r in range(R):
    for kind, script in KINDS.items():
        a = token_counts(); t0 = time.time()
        subprocess.run([bash, "-c", script], creationflags=0x08000000, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(2); b = token_counts(); dt = time.time() - t0
        c0 = token_counts(); time.sleep(dt); c1 = token_counts()
        d = (b[0] - a[0]) - (c1[0] - c0[0])
        per = d / N if kind != "bash-only" else float(d)
        print(f"round {r} {kind:14} {dt:5.1f} s batch {b[0]-a[0]:+5d} idle {c1[0]-c0[0]:+5d} net {d:+5d} per child {per:+.2f}", flush=True)
print("end Token", token_counts())
