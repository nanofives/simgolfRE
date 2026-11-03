"""P5A item 2: the ORIGINAL's theme-pack screen with no scenario agent on the main thread (U-1260: the column-5 stack word).
Boots instances/p5a with re/tools/game.py (harness.js only: rpc calls on Frida's own thread), clicks Select A Theme on the
title menu, takes the window frame. Run: SIMGOLF_ALLOW_MULTI=1 SIMGOLF_GAME_DIR=instances/p5a ~/bin/py -3.12 <this> <n>"""
import os, sys, time, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[3] / "re" / "tools"))
from game import Game

out = pathlib.Path(__file__).resolve().parent / "P5A_img"
runs = int(sys.argv[1]) if len(sys.argv) > 1 else 1
for k in range(runs):
    with Game(env={"SIMGOLF_SKIP_INTRO": "1"}) as g:
        g.start()
        g.wait_window(30)
        time.sleep(8)                       # title menu up
        g.shot(out / f"theme_title_{k}.png")
        for k2 in range(6):                 # hover Select A Theme first (the menu's selection starts at 1 until a hover)
            g.move(630 + k2, 423)
            time.sleep(0.5)
        g.shot(out / f"theme_hover_{k}.png")
        g.click(630, 423, settle=1.0)       # Select A Theme
        time.sleep(4)
        g.shot(out / f"theme_packs_{k}.png")
        g.move(400, 300)                    # cursor elsewhere, one more frame
        time.sleep(2)
        g.shot(out / f"theme_packs_{k}b.png")
    print("run", k, "done")
