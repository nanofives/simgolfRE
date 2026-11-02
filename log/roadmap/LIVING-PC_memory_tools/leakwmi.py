"""Token objects kept per WMI query (Get-CimInstance inside ONE powershell), against an idle control of equal length.
Usage: ~/bin/py -3.12 leakwmi.py [N] [rounds]"""
import subprocess, time, sys
sys.path.insert(0, __file__.rsplit("\\", 1)[0].rsplit("/", 1)[0])
from leakkinds_lib import token_counts
N = int(sys.argv[1]) if len(sys.argv) > 1 else 40
R = int(sys.argv[2]) if len(sys.argv) > 2 else 2
KINDS = {
    "ps-only": "exit 0",
    "cim OS": f"1..{N} | ForEach-Object {{ $null = Get-CimInstance Win32_OperatingSystem }}",
    "cim Process": f"1..{N} | ForEach-Object {{ $null = Get-CimInstance Win32_Process }}",
    "get-process": f"1..{N} | ForEach-Object {{ $null = Get-Process }}",
}
print(f"N={N} rounds={R} start Token {token_counts()}", flush=True)
for r in range(R):
    for kind, cmd in KINDS.items():
        a = token_counts(); t0 = time.time()
        subprocess.run(["powershell.exe", "-NoProfile", "-Command", cmd], creationflags=0x08000000,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(2); b = token_counts(); dt = time.time() - t0
        c0 = token_counts(); time.sleep(dt); c1 = token_counts()
        d = (b[0] - a[0]) - (c1[0] - c0[0])
        print(f"round {r} {kind:12} {dt:5.1f} s batch {b[0]-a[0]:+5d} idle {c1[0]-c0[0]:+5d} net {d:+5d} per query {d / (1 if kind == 'ps-only' else N):+.2f}", flush=True)
print("end Token", token_counts())
