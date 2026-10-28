"""S3.13 (copy of I9's): gfx.check() on the mirror's DLLs (run from the mirror root): duplicate body names, the body count, the not-drawing count,
and the owner of the names this merge's families share with others (Surface_drawSprite, Widget_fillRect, drawStringRightAt,
drawWindow.frame, drawWindow.golfer). One process per backend (SG_BACKEND selects lib.load's backend; I6's script passed a
`backend` argument lib.load does not take).
Usage: ~/bin/py -3.12 log/roadmap/S3/13_gfx_check.py [x64] [win32]"""
import os
import pathlib
import subprocess
import sys

NAMES = ("Surface_drawSprite", "Widget_fillRect", "drawStringRightAt", "drawWindow.frame", "drawWindow.golfer")


def one():
    sys.path.insert(0, str(pathlib.Path.cwd()))
    from port.test import lib as L
    lib = L.load()
    with lib.state() as st:
        st.call("gfx.attach", [])
        n = st.call("gfx.check", []).eax
        rows = st.files_read("$gfx/bodies.txt").decode().splitlines()
    body = [r for r in rows if r.startswith("body\t")]
    nd = [r for r in rows if r.startswith("not_drawing\t")]
    dup = [r for r in rows if r.startswith("duplicate\t")]
    print(os.environ.get("SG_BACKEND", "x64"), "duplicates", n, "bodies", len(body), "not_drawing", len(nd))
    for r in dup:
        print("  " + r)
    for r in body:
        if r.split("\t")[1] in NAMES:
            print("  " + r)


def main():
    if os.environ.get("I9_GFX_CHILD"):
        one()
        return
    for be in sys.argv[1:] or ["x64"]:
        env = dict(os.environ, I9_GFX_CHILD="1")
        if be == "x64":
            env.pop("SG_BACKEND", None)
        else:
            env["SG_BACKEND"] = be
        subprocess.run([sys.executable, __file__], env=env, check=False)


if __name__ == "__main__":
    main()
