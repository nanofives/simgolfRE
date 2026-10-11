"""Deep canonical scenarios (re/tools/scenario.py): building and opening a hole, time warp, championship."""
import time

import pytest

import imgcmp
import scenario
from conftest import GOLDEN
from game import Game

pytestmark = [pytest.mark.runtime, pytest.mark.slow]
DATE_LINE = (90, 18, 210, 34)   # the "Month Year" line under the course name
# The game date (g_date): appendDate 0x0040d7b0 decodes year = date / 8192 (+2001) and month = date / 1024 % 8;
# drawEmployeeDetailPanel passes this dword to it (0x00436e50) and logTick 0x0040c6f0 writes it.
GAME_DATE = 0x00834170
MONTH = 1024
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
        start = g.read_u32(GAME_DATE)
        scenario.run_season(g, 45)
        end = g.read_u32(GAME_DATE)
        assert g.alive
    # Read the date instead of comparing the date line's pixels: an exact one-year advance (June 2001 -> June 2002)
    # changes one digit, which scored ~0.012 against the old 0.02 changed-pixel threshold and failed 1-3 runs in 8
    # with or without hooks (rounds 22 and 27, log/c3/alt/timewarp_bisect*.py). 45 s at x32 advanced ~1.4 years
    # (0x0e97 -> 0x2b0a, 2026-10-11); require at least three game months.
    assert end - start >= 3 * MONTH, f"date moved from {start:#x} to {end:#x}"


def test_championship_round_is_played_to_the_results():
    with _fresh({"SIMGOLF_TIMEWARP": "8"}) as g:
        g.wait_window()
        imgcmp.wait_for(g, imgcmp.load(GOLDEN / "main_menu.png"), timeout=30)
        scenario.start_championship(g)
        shot = g.shot()
        assert imgcmp.matches(shot.crop(LEADERBOARD), imgcmp.load(GOLDEN / "leaderboard_header.png"), 30)
        result = scenario.play_tournament(g, 240)
        assert g.alive
    # Gary's shots are driven by the harness (straight shot at the detected green); the round must finish.
    assert result["finished"], f"no TOURNAMENT RESULTS after {len(result['shots'])} shots"
    assert len(result["shots"]) >= 1


def test_golfer_panel_and_landmark_injection():
    """Organic: clicking a golfer opens the info panel (0x0045c560 called with the golfer index).
    Injected (declared non-organic, U-0003): the landmark-available notice 0x004722c0 runs on the game
    thread for that golfer without crashing the game."""
    if Game.running_instances():
        pytest.fail("SimGolf already running")
    with Game(env={"SIMGOLF_SKIP_INTRO": "1", "SIMGOLF_TIMEWARP": "8"}, extra_js=["events.js"]) as g:
        g.wait_window()
        scenario.to_sandbox(g)
        scenario.build_first_hole(g)
        scenario.run_season(g, 25)
        golfer = scenario.open_golfer_panel(g)
        assert 0 <= golfer < 256
        g.click(592, 258)
        time.sleep(1)
        result = scenario.inject_landmark_notice(g, golfer)
        assert result and result[-1][3] == "ok", result
        time.sleep(3)
        assert g.alive


@pytest.mark.slow
def test_golfer_stories_advance_organically():
    """Organic (U-0003): on a multi-hole course FUN_00466370 advances a golfer-pair story chapter (returns
    non-zero), which it never does on the 1-hole course (it returns 0 while the golfer's hole, char +0x21, is < 2).
    The sandbox terrain is random and can refuse holes, so build_four_holes retries fallback spots until at
    least 2 holes are open (read from 0x5685f0); the season gets 240 s."""
    if Game.running_instances():
        pytest.fail("SimGolf already running")
    spec = scenario.SCENARIOS["golfer_stories"]
    with Game(env={"SIMGOLF_SKIP_INTRO": "1", **spec["env"]}, extra_js=spec["js"]) as g:
        g.wait_window()
        scenario.to_sandbox(g)
        holes = scenario.build_four_holes(g)
        assert holes >= 2, f"only {holes} hole(s) opened on this terrain"
        scenario.story_season(g, seconds=240)
        assert g.extra["advanced"], g.extra
        assert g.alive


@pytest.mark.slow
@pytest.mark.parametrize("name, fixture", [("stories_fixture", scenario.STORIES_FIXTURE),
                                           ("stories9_fixture", scenario.STORIES9_FIXTURE),
                                           ("mood5_fixture", scenario.MOOD5_FIXTURE)])
def test_stories_fixture_loads_and_advances(name, fixture):
    """The 5-hole, 9-hole and fairway (mood5) saves load through Continue Saved Game (the 'saved games' folder
    exists) and a pair-story chapter advances organically within the season."""
    if not fixture.exists():
        pytest.skip(f"{fixture.name} not built (see tests/fixtures/stories/README.md)")
    if Game.running_instances():
        pytest.fail("SimGolf already running")
    spec = scenario.SCENARIOS[name]
    with Game(env={"SIMGOLF_SKIP_INTRO": "1", **spec["env"]}, extra_js=spec["js"]) as g:
        g.wait_window()
        for _, step in spec["phases"]:
            step(g)
        assert g.extra["advanced"], g.extra
        assert g.alive
