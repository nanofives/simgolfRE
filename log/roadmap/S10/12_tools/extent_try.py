"""S10.12: score a MATCH line with a different original extent (monkeypatch of match.ghidra_size; no shared file edited).

    ~/bin/py -3.12 log/roadmap/S10/12_tools/extent_try.py <src> <addr hex> <size> [--show]
"""
import contextlib, io, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402

src, addr, size = pathlib.Path(sys.argv[1]), int(sys.argv[2], 16), int(sys.argv[3])
real = match.ghidra_size
match.ghidra_size = lambda mod, a: size if a == addr else real(mod, a)
buf = io.StringIO()
with contextlib.redirect_stdout(buf):
    match.compare(src, "", "--show" in sys.argv, only=addr)
print(buf.getvalue())
