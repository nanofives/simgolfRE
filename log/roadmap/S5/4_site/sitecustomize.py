"""S5.4: PYTHONPATH=log/roadmap/S5/4_site loads the patched segments / boot / integrator copies (log/roadmap/S5/4_harness) as
port.test.<name> in every Python process (also the spawned workers of integrator's process pool). Only when S5_4_PATCH=1."""
import os
import sys

if os.environ.get("S5_4_PATCH") == "1":
    import importlib.util
    import pathlib

    here = pathlib.Path(__file__).resolve().parent.parent
    root = here.parents[2]
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))
    import port.test  # noqa: F401
    for n in ("segments", "boot", "integrator"):
        full = f"port.test.{n}"
        spec = importlib.util.spec_from_file_location(full, here / "4_harness" / f"{n}.py")
        mod = importlib.util.module_from_spec(spec)
        sys.modules[full] = mod
        spec.loader.exec_module(mod)
        setattr(sys.modules["port.test"], n, mod)
