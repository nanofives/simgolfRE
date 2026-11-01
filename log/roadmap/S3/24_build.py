"""S3.24: build the S3_24 tree's portsim (x64 or Win32) through the tree's port/test/build.py (cmake from Python, FreeType on).

    ~/bin/py -3.12 log/roadmap/S3/24_build.py [x64|Win32] [--tree S3_24]     (heavy: take log/roadmap/S3/w8_heavy.lock)
"""
import os, pathlib, subprocess, sys
ROOT = pathlib.Path(__file__).resolve().parents[3]
arch = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else "x64"
tree = sys.argv[sys.argv.index("--tree") + 1] if "--tree" in sys.argv else "S3_24"
T = ROOT / "port" / "build" / tree
env = dict(os.environ, SG_FREETYPE_ROOT=r"C:\Users\maria\vcpkg\installed")
r = subprocess.run([sys.executable, str(T / "port" / "test" / "build.py"), "--arch", arch], cwd=T, env=env)
sys.exit(r.returncode)
