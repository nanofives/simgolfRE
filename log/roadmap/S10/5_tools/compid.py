"""S10.5: which compiler build wrote an obj. Prints @comp.id (prodid << 16 | build) and cl's banner per toolchain.

    ~/bin/py -3.12 log/roadmap/S10/5_tools/compid.py
"""
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402


def comp_id(obj: pathlib.Path) -> tuple[int, int]:
    _, syms = match.coff(obj)
    s = next(s for s in syms if s["name"] == "@comp.id")
    return s["value"] >> 16, s["value"] & 0xFFFF


def banner(tc: str) -> str:
    t = match.TOOLCHAINS[tc]
    env = {"PATH": ";".join(str(d) for d in [t["bin"], *t["path"]]), "SystemRoot": r"C:\Windows"}
    r = subprocess.run([str(t["bin"] / "cl.exe")], capture_output=True, text=True, env=env)
    return (r.stderr or r.stdout).splitlines()[0]


def main():
    d = pathlib.Path(tempfile.mkdtemp(prefix="s10_5_"))
    cpp, c = d / "t.cpp", d / "t.c"
    cpp.write_text("int f(int a) { return a * 3 + 1; }\n")
    c.write_text("int g(int a) { return a * 5 + 2; }\n")
    for tc in match.TOOLCHAINS:
        ids = [comp_id(match.compile_obj(src, "/O2", lang, "", tc)) for src, lang in ((cpp, "c++"), (c, "c"))]
        print(f"{tc:5} banner: {banner(tc)}")
        print(f"{tc:5} @comp.id C++ obj: prodid {ids[0][0]} build {ids[0][1]}   C obj: prodid {ids[1][0]} build {ids[1][1]}")


if __name__ == "__main__":
    main()
