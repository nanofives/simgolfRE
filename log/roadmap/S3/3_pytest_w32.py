"""S3.3: run port/test/test_gfx_eoi.py on the Win32 backend against the S3_3_w32 build (run from the mirror root port/build/S3_head)."""
import os
import pathlib
import sys

ROOT = pathlib.Path.cwd()
sys.path.insert(0, str(ROOT))
os.environ["SG_BACKEND"] = "win32"
from port.test import build as B  # noqa: E402

dll32 = ROOT / "port/build/S3_3_w32/Release/portsim_c.dll"
run32 = ROOT / "port/build/S3_3_w32/Release/portsim_runner32.exe"
orig = B.dll_path
B.dll_path = lambda arch=None, config="Release": dll32 if (arch or B.default_arch()) == "Win32" else orig(arch, config)
B.runner32_path = lambda config="Release": run32
B.runner32_stale = lambda config="Release": not (run32.exists() and dll32.exists())
import pytest  # noqa: E402

sys.exit(pytest.main(["port/test/test_gfx_eoi.py", "-q", "-p", "no:cacheprovider"]))
