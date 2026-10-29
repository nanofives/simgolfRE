"""S10.9: re-run m36e_lib_match.py for one function with SMSG's extent instead of ours (process-local override of
match.boundary_overrides, no file written outside --out). Tests whether a library miss is only our extent.
~/bin/py -3.12 log/roadmap/S10/9_tools/libid_extent.py <lib> <module> 0x<va>=<symbol> <size> --out <dir>"""
import pathlib, runpy, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402

lib, mod, pair, size = sys.argv[1:5]
va = int(pair.split("=")[0], 16)
key = match.tracker_key(mod, va)
orig = match.boundary_overrides


def patched(module):
    ov = dict(orig(module))
    if module == mod:
        ov[key] = int(size)
    return ov


match.boundary_overrides = patched
sys.argv = ["m36e_lib_match.py", lib, mod, pair] + sys.argv[5:]
runpy.run_path(str(ROOT / "log/c3/m36e_lib_match.py"), run_name="__main__")
