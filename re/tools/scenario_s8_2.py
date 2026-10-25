"""Roadmap S8.2 scenarios (log/roadmap/S8/2_notes.md). Registered by re/tools/scenario.py's end-of-file hook (it globs
scenario_s[18]_*.py); re/tools/s8_movie_runs.py also imports this module explicitly before scenario_refs.setup runs.
Names start with cov_r so scenario_refs' s36a family runs them; `expect_exit` makes setup load c3fya_run's exit support.

cov_r_s8_intro: process start to the title menu and a short stay on it. Meant to run with SIMGOLF_SKIP_INTRO=0 (SMSG_IntroFinal.bik
played by binkw32 from showTitleScreen 0x0043cd97 through playAudioFile 0x0043cce0) and, for the baseline, with the same script and
SIMGOLF_SKIP_INTRO=1 (the shim answers NULL for "introfinal").
  intro_early   a fixed virtual time after the process start (inside the movie with the intro ON, on the way to the menu with it OFF)
  menu          the first checkpoint after the main-menu screen matched (3 s settle, like cov_a_quit_title)
  menu_late     5 virtual s later

cov_r_s8_full: the same start, then cov_a_quit_title's tail: the title menu's exit icon (mainLoop QUIT 0x0040fa29, closing movie
at the call site 0x0040faa4, showCreditsScreen 0x0044b9c0), a checkpoint every 2 s through the closing movie and the credits,
Esc, and the process exit."""
import scenario as SC

imgcmp = SC.imgcmp
GOLDEN = SC.GOLDEN
SCENARIOS = SC.SCENARIOS


def _start(c):
    yield from c.sleep(2000)
    yield from c.checkpoint("intro_early")
    yield from c.wait_screen(imgcmp.load(GOLDEN / "main_menu.png"), name="main_menu")
    yield from c.sleep(3000)
    yield from c.checkpoint("menu")


def cov_r_s8_intro(c):
    yield from _start(c)
    yield from c.sleep(5000)
    yield from c.checkpoint("menu_late")
    for _ in range(3):
        yield from c.sleep(1000)


def cov_r_s8_full(c):
    yield from _start(c)
    c.move(750, 553); yield from c.sleep(1500)
    yield from c.checkpoint("hover_exit")
    c.run.exit_expected = True
    yield from c.click(750, 553); yield from c.sleep(2500)
    for i in range(10):
        yield from c.checkpoint(f"quit_{i:02d}")
        yield from c.sleep(2000)
    yield from c.checkpoint("credits")
    yield from c.key(0x1B); yield from c.sleep(1500)             # the credits end on a key or a mouse button
    for _ in range(300):
        yield from c.sleep(1000)


cov_r_s8_full.expect_exit = True
SCENARIOS["cov_r_s8_intro"] = {"env": {}, "phases": [], "lockstep": cov_r_s8_intro}
SCENARIOS["cov_r_s8_full"] = {"env": {}, "phases": [], "lockstep": cov_r_s8_full}
