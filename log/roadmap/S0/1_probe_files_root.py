"""S0.1 probe for U-1101: does the Win32 runner's files_root write through to disk?  Run from the export root with SG_BACKEND=win32."""
import os
import pathlib
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[3]
if not (ROOT / "port").exists():
    ROOT = pathlib.Path(os.getcwd())
sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402

for be in ("x64", "win32"):
    os.environ["SG_BACKEND"] = be
    L._LIB = None
    lib = L.load()
    root = pathlib.Path(tempfile.mkdtemp(prefix="s01probe_"))
    with lib.state() as st:
        r = st.files_root(root)
        print(be, "files_root returned", r)
        st.files_put("Saved Games/x.sve", b"hello")
        print(be, "size via ABI", st.files_size("Saved Games/x.sve"), "read", st.files_read("Saved Games/x.sve"))
    found = sorted(str(p.relative_to(root)) for p in root.rglob("*") if p.is_file())
    print(be, "on disk:", found)
