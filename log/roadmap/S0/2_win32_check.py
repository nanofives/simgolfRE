"""S0.2: framegate rows on the Win32 backend from step S0.2's own Win32 build (port/build/S0_2_w32), x64 rows for comparison.
Run from the HEAD export root: SG_BACKEND=win32 ~/bin/py -3.12 log/roadmap/S0/2_win32_check.py <scenario> <chk,...>
The build helpers are pointed at S0_2_w32 so the export's default Win32 dir (S0.1's) is neither read nor rebuilt."""
import json
import os
import pathlib
import sys

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
from port.test import build as B  # noqa: E402

W32 = ROOT / "port" / "build" / "S0_2_w32" / "Release"
B.runner32_path = lambda config="Release": W32 / "portsim_runner32.exe"
_dll = B.dll_path
B.dll_path = lambda arch=None, config="Release": W32 / "portsim_c.dll" if arch == "Win32" else _dll(arch, config)
B.runner32_stale = lambda config="Release": False
B.stale = lambda arch=None, config="Release": False
from port.test import framegate as FG  # noqa: E402

rows = FG.run_scenario(sys.argv[1], sys.argv[2].split(","), probe=False)
keys = ("checkpoint", "status", "detail", "kind", "total_px", "one_level_px", "beyond_px", "regions", "unsup_before", "boot_status",
        "boot_records", "liveterrain", "backend")
print(json.dumps([{k: r.get(k) for k in keys} for r in rows]))
