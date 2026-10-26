"""S5.3: port/test/test_static.py's source checks run over the S5.3 mirror tree (port/build/S5_3_src: HEAD + S5.1 / S5.2 / S5.3
files + the merge lines), since the shared tree holds siblings' unmerged modules that fail test_every_module_file_is_listed."""
import inspect
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from port.test import test_static as T  # noqa: E402

T.ROOT = ROOT / "port" / "build" / "S5_3_src"
T.PORT = T.ROOT / "port"
bad = 0
for name, fn in inspect.getmembers(T, inspect.isfunction):
    if not name.startswith("test_") or inspect.signature(fn).parameters:
        continue
    try:
        fn()
        print("PASS", name)
    except Exception as e:   # noqa: BLE001
        bad += 1
        print("FAIL", name, str(e)[:400])
sys.exit(1 if bad else 0)
