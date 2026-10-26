"""I7: does a state + gditext.attach / detach cycle leave memory behind in the Win32 runner? Prints the runner's private bytes
after k cycles, with and without the attach (the same set-up live.py's boot_state makes, minus the run).
Run from a mirror root: SG_BACKEND=win32 SG_PORTSIM_W32_DIR=<root>/port/build/Win32 ~/bin/py -3.12 <this> [cycles]"""
import os
import sys

sys.path.insert(0, os.getcwd())
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from port.test import lib as L, segments as SG  # noqa: E402
from runner_res_probe import res  # noqa: E402

n = int(sys.argv[1]) if len(sys.argv) > 1 else 10
lib = L.load()
lib.exe_load()
pid = lib.runner.proc.pid if hasattr(lib, "runner") else os.getpid()
print("start", res(pid), flush=True)
for attach in (False, True):
    for k in range(n):
        with lib.state() as st:
            st.set_data_base(SG.DATA_VA)
            st.fill_from_exe(True)
            if SG.INSTALL.is_dir():
                st.files_root(SG.INSTALL)
            if attach:
                st.call("gditext.attach", [])
                st.call("gditext.detach", [])
    print("attach" if attach else "no attach", n, "cycles:", res(pid), flush=True)
