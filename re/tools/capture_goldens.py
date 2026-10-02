"""Regenerate the game-derived test assets from YOUR copy of the game (they are not in the public repo).

    py -3.12 re/tools/capture_goldens.py            # all: goldens + championship fixture
    py -3.12 re/tools/capture_goldens.py --only goldens|championship

Produces:
  tests/golden/main_menu.png, location.png, ingame_sandbox.png, ingame_hud_dial.png, ingame_hud_logo.png,
  leaderboard_header.png
  tests/fixtures/championship/  (a 1-hole course built and saved with "Save Course for Championship")

Every capture is preceded by a wait on a structural condition, and the script prints what it captured;
look at the PNGs once before trusting them (a wrong golden makes every runtime test lie).
"""
import argparse
import pathlib
import shutil
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
from game import Game  # noqa: E402
import scenario  # noqa: E402

G = ROOT / "tests" / "golden"
FIX = ROOT / "tests" / "fixtures" / "championship"
CHAMP = ROOT / "original" / "Themes" / "Championship"


def stable_shot(g, settle=1.0, tries=20):
    """Wait until two consecutive captures agree (animation settled), then return it."""
    import imgcmp
    prev = g.shot()
    for _ in range(tries):
        time.sleep(settle)
        cur = g.shot()
        if imgcmp.distance(prev, cur) < 2:
            return cur
        prev = cur
    return prev


def goldens():
    G.mkdir(parents=True, exist_ok=True)
    with Game(env={"SIMGOLF_SKIP_INTRO": "1"}) as g:
        g.wait_window()
        time.sleep(6)
        stable_shot(g).save(G / "main_menu.png")
        g.click(130, 445)
        time.sleep(3)
        stable_shot(g).save(G / "location.png")
        g.click(270, 32)
        time.sleep(18)
        im = g.shot()
        im.save(G / "ingame_sandbox.png")
    im.crop(scenario.DIAL).save(G / "ingame_hud_dial.png")
    im.crop((18, 18, 58, 58)).save(G / "ingame_hud_logo.png")
    print("goldens: main_menu, location, ingame_sandbox (+ dial/logo crops)")


def championship():
    before = set(CHAMP.iterdir())
    with Game(env={"SIMGOLF_SKIP_INTRO": "1"}) as g:
        g.wait_window()
        scenario.to_sandbox(g)
        scenario.build_first_hole(g)
        g.click(130, 580)          # wrench (system) menu
        time.sleep(1.2)
        g.click(250, 415)          # Save Course for Championship
        time.sleep(3)
    new = [p for p in CHAMP.iterdir() if p not in before]
    if not new:
        raise SystemExit("no championship course was written")
    FIX.mkdir(parents=True, exist_ok=True)
    for f in FIX.iterdir():
        f.unlink()
    for p in new:
        shutil.copy2(p, FIX / p.name)
    print("championship fixture:", [p.name for p in new])
    with Game(env={"SIMGOLF_SKIP_INTRO": "1"}) as g:
        g.wait_window()
        import imgcmp
        imgcmp.wait_for(g, imgcmp.load(G / "main_menu.png"), timeout=30)
        scenario.start_championship(g)
        g.shot().crop((4, 2, 140, 22)).save(G / "leaderboard_header.png")
    print("golden: leaderboard_header")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", choices=["goldens", "championship"])
    a = ap.parse_args()
    if a.only in (None, "goldens"):
        goldens()
    if a.only in (None, "championship"):
        championship()


if __name__ == "__main__":
    main()
