"""Deep canonical scenarios (re/tools/scenario.py): building and opening a hole, time warp, championship."""
import time

import pytest

import imgcmp
import scenario
from conftest import GOLDEN
from game import Game

pytestmark = [pytest.mark.runtime, pytest.mark.slow]
DATE_LINE = (90, 18, 210, 34)   # the "Month Year" line under the course name
LEADERBOARD = (4, 2, 140, 22)


def _fresh(env):
    if Game.running_instances():
        pytest.fail("SimGolf already running")
    return Game(env={"SIMGOLF_SKIP_INTRO": "1", **env})


def test_time_warp_moves_the_calendar():
    """x32 warp + Esc to clear event popups: months pass in seconds (3 game years per 2 real minutes)."""
    with _fresh({"SIMGOLF_TIMEWARP": "32"}) as g:
        g.wait_window()
        scenario.to_sandbox(g)
        scenario.build_first_hole(g)
        start = g.shot().crop(DATE_LINE)
        scenario.run_season(g, 45)
        end = g.shot().crop(DATE_LINE)
        assert g.alive
    # Mean grayscale distance diluted a real change (June 2001 -> April 2002 scored 2.9); count strongly
    # changed pixels instead. Measured: 0 for identical frames, several percent for a new month/year.
    assert imgcmp.changed_fraction(start, end) > 0.02, "date text did not change"


def test_championship_reaches_the_leaderboard():
    with _fresh({"SIMGOLF_TIMEWARP": "8"}) as g:
        g.wait_window()
        imgcmp.wait_for(g, imgcmp.load(GOLDEN / "main_menu.png"), timeout=30)
        scenario.start_championship(g)
        shot = g.shot()
        assert imgcmp.matches(shot.crop(LEADERBOARD), imgcmp.load(GOLDEN / "leaderboard_header.png"), 30)
        scenario.run_season(g, 20)
        assert g.alive
