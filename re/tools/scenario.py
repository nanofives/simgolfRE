"""Canonical scenario steps shared by tests, census, recorder. Coordinates are 800x600 game space."""
import pathlib
import time

import imgcmp

ROOT = pathlib.Path(__file__).resolve().parents[2]
GOLDEN = ROOT / "tests" / "golden"
DIAL = (15, 535, 140, 595)


def to_sandbox(g, settle=2.0):
    """main menu -> Sandbox Mode -> Monterey -> in game (HUD visible)."""
    imgcmp.wait_for(g, imgcmp.load(GOLDEN / "main_menu.png"), timeout=60)
    g.click(130, 445)
    imgcmp.wait_for(g, imgcmp.load(GOLDEN / "location.png"), timeout=15)
    g.click(270, 32)
    dial = imgcmp.load(GOLDEN / "ingame_hud_dial.png")
    t0 = time.time()
    while not imgcmp.matches(g.shot().crop(DIAL), dial, 30):
        if time.time() - t0 > 60:
            raise TimeoutError("sandbox HUD never appeared")
        time.sleep(0.5)
    time.sleep(settle)


# Build Course palette (bottom bar, opened by the big golf icon). Tools found by hover tooltips.
GOLF_ICON = (45, 470)
TOOL_TEE = (300, 527)      # "Tees $100"
TOOL_GREEN = (365, 527)    # "Green $1000"
VK_H = 0x48                # "Open Hole for Business" (manual hotkey table; in-game text renders it as 'f')


def build_first_hole(g, tee=(300, 200), green=(520, 330)):
    """Place a tee and a green (hole 1, ~175 yd par 3 on Monterey), drop the tool, open the hole."""
    g.click(*GOLF_ICON); time.sleep(1)
    g.click(*TOOL_TEE); time.sleep(0.8)
    g.click(*tee); time.sleep(1.2)
    g.click(*TOOL_GREEN); time.sleep(0.8)
    g.click(*green); time.sleep(2)
    g.click(300, 350, right=True); time.sleep(1)   # right click drops the build tool
    g.key(VK_H); time.sleep(3)


def run_season(g, seconds, esc_every=2.0):
    """Let game time run (use with SIMGOLF_TIMEWARP); Esc clears event popups (SimFoto, golfer news),
    which otherwise hold game time until dismissed. At x32, ~1 game year per 40 s."""
    t0 = time.time()
    while time.time() - t0 < seconds:
        g.key(0x1B)
        time.sleep(esc_every)
        if not g.alive:
            raise RuntimeError(f"game died during season: {g.detach_reason}")


# Named scenarios: phases are (name, callable(g)) run in order; the census switches the coverage phase bit
# before each one. env is passed to the shim.
SCENARIOS = {
    "sandbox_basic": {
        "env": {},
        "phases": [("menu", lambda g: time.sleep(3)),
                   ("sandbox", lambda g: (to_sandbox(g), time.sleep(40)))],
    },
    "course_season": {
        "env": {"SIMGOLF_TIMEWARP": "32"},
        "phases": [("menu", lambda g: time.sleep(3)),
                   ("sandbox", lambda g: to_sandbox(g)),
                   ("build_hole", lambda g: build_first_hole(g)),
                   ("season", lambda g: run_season(g, 240))],
    },
}


# ---- Championship (tournament) -------------------------------------------------------------------
# The fixture is a 1-hole course built by build_first_hole() and saved with the wrench menu's
# "Save Course for Championship" (2026-10-02). Installing it makes "Play a Championship" list it.
import shutil  # noqa: E402

CHAMP_FIXTURE = ROOT / "tests" / "fixtures" / "championship"
CHAMP_DIR = ROOT / "original" / "Themes" / "Championship"


def install_championship_course():
    for f in CHAMP_FIXTURE.iterdir():
        shutil.copy2(f, CHAMP_DIR / f.name)


def start_championship(g):
    """main menu -> Play a Championship -> Easy -> course 1 -> pro 1 -> tournament running."""
    install_championship_course()
    g.click(400, 540); time.sleep(2.5)     # Play a Championship
    g.click(390, 125); time.sleep(3)       # Easy
    g.click(380, 122); time.sleep(1)       # first course in the list
    g.click(630, 555); time.sleep(4)       # OK
    g.click(360, 122); time.sleep(1)       # Gary Golf.pro
    g.click(632, 553); time.sleep(8)       # OK -> tournament (leaderboard top-left)


SCENARIOS["championship"] = {
    "env": {"SIMGOLF_TIMEWARP": "8"},
    "phases": [("menu", lambda g: time.sleep(3)),
               ("championship_setup", lambda g: start_championship(g)),
               ("tournament", lambda g: run_season(g, 90))],
}
