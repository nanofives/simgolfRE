"""S3.4b: run test files on the Win32 backend against the S3_4b_w32 build (run from port/build/S3_head):
    SG_BACKEND=win32 ~/bin/py -3.12 ../../../log/roadmap/S3/4b_run_w32.py port/test/test_defects_flic.py [pytest args]
Points port.test.build at the private build the way port/test/integrator.py does for --dll32 / --runner32."""
import pathlib
import sys

sys.path.insert(0, ".")
import pytest  # noqa: E402

from port.test import build as B  # noqa: E402

DIR = pathlib.Path("port/build/S3_4b_w32/Release").resolve()
dll, run = DIR / "portsim_c.dll", DIR / "portsim_runner32.exe"
orig = B.dll_path
B.runner32_path = lambda config="Release": run
B.dll_path = lambda arch=None, config="Release": dll if (arch or B.default_arch()) == "Win32" else orig(arch, config)
B.runner32_stale = lambda config="Release": not (run.exists() and dll.exists())
sys.exit(pytest.main(sys.argv[1:] + ["-q"]))
