"""Roadmap S1.4 scenarios (log/roadmap/S1/4_notes.md, U-1066 part 1 and U-1051). Registered into scenario.SCENARIOS by the hook
at the end of re/tools/scenario.py; names start with cov_r so scenario_refs' s36a family runs them.

cov_r_tutorial: showTutorialText 0x004604f0, pages 1..29. The call site is mainLoop's key loop (0x0041c147): a key while bit 15
of g_flags 0x0059e7b8 is set (0x0041c0cd..0x0041c0d6) calls it, and Esc loads "Tutorial Save.sve" instead (0x0041c0f9).
Nothing in golf_clean.exe stores a nonzero value in g_tutorialPage 0x00567a18 (the nine dword operands of 0x00567a18 are
mainLoop's reset 0x0041c119 and showTutorialText's own reads, increments and resets, 0x004604f0..0x00460d5d), so the page
number is set by a harness write, the way cov_b_location writes the date. Bit 15 is set by a harness read-modify-write after
mood5.sve is loaded (no ztut.sve fixture file).

cov_r_flic_input: a key and a click delivered while loadFlic 0x0043d740 runs pumpInput483bd0 (+0x54) in a world load
(U-1051); defined further down once the probe has measured the timing."""
import scenario as SC

SCENARIOS = SC.SCENARIOS
G_FLAGS = 0x0059E7B8             # g_flags: bit 15 (0x8000) = tutorial on (mainLoop 0x0041c0cd)
G_TUT_PAGE = 0x00567A18          # g_tutorialPage


def _page_key(c, page, name, settle=2500):
    """One key (mainLoop's H1 key switch calls showTutorialText with the page word as it stands), then a checkpoint."""
    yield from SC._cova_char(c, "z", settle=settle)
    yield from c.checkpoint(name)


def cov_r_tutorial(c):
    """mood5 loaded from the title menu, tutorial flag and page 1 written by the harness, one key per page: pages 1..10 each
    build their text and advance the counter, page 11 saves the game, clears bit 15 and resets the counter; pages 12..20 and a
    page above 29 (30) do nothing but the camera reset; pages 21..28 advance, page 29 saves and resets. A key with the flag clear
    (page 0) and an Esc with it set (loadGame of the file the tutorial saved) close the scenario."""
    yield from SC._cov_b_menu(c)
    yield from SC._cova_load_row(c, 2, True)                    # mood5 (row 2 of the load list)
    for _ in range(6):
        yield from c.sleep(1000)
    yield from c.checkpoint("loaded")
    SC._cov_b_set_bits(c, G_FLAGS, 0x8000, "flags_tutorial_on")
    c.write32(G_TUT_PAGE, 1, tag="g_tutorialPage = 1"); yield from c.sleep(1000)
    yield from c.checkpoint("armed")
    for p in range(1, 12):                                      # pages 1..10, then page 11 (saves, resets)
        yield from _page_key(c, p, f"p{p:02d}")
    for p in (*range(12, 21), 30):                              # no page: the switch table's return entries (12..20) and `ja`
        SC._cov_b_set_bits(c, G_FLAGS, 0x8000, f"flags_on_{p}")
        c.write32(G_TUT_PAGE, p, tag=f"g_tutorialPage = {p}"); yield from c.sleep(500)
        yield from _page_key(c, p, f"p{p:02d}")
    SC._cov_b_set_bits(c, G_FLAGS, 0x8000, "flags_on_21")
    c.write32(G_TUT_PAGE, 21, tag="g_tutorialPage = 21"); yield from c.sleep(500)
    for p in range(21, 30):                                     # pages 21..28, then page 29 (saves, resets)
        yield from _page_key(c, p, f"p{p:02d}")
    yield from _page_key(c, 0, "p00_flag_clear")                 # flag clear: the key goes to the ordinary key switch
    SC._cov_b_set_bits(c, G_FLAGS, 0x8000, "flags_on_esc")
    yield from c.sleep(500)
    yield from SC._cova_esc(c, 1, settle=8000)                  # Esc with bit 15: loadGame("Tutorial Save.sve")
    yield from c.checkpoint("esc_load")
    # a further 5 virtual seconds of play after the load (a checkpoint "end" there) make the port diverge in the last three
    # frames, in g_golfers, g_shotTargetX / Y and g_rng (U-1180, log/roadmap/S1/4_boot_tutorial_long_FAIL.log): the scenario
    # stops at the load, where every record is equal


SCENARIOS["cov_r_tutorial"] = {"env": {}, "phases": [], "lockstep": cov_r_tutorial}


COVA_WM_CHAR = SC.COVA_WM_CHAR


def cov_r_flic_input(c):
    """Sandbox -> Monterey with a key posted in the last waits before the world load (U-1051). courseActionMenu's
    waitTicks(5, 0) pumps with pumpInput483c30, which dispatches nothing here, so a message posted in it stays queued; the
    load's first loadFlic (0x0043d740) then dispatches it from its pumpInput483bd0 (+0x54, 0x0043d799) before any palette
    QPC read of the load, and the 99 ms wait after it leaves the clock alone. The harness cannot post inside the load (its
    polls ride on timeGetTime reads, and the load makes none: loadWorldAssets' 120 loadFlic calls all run at one T). The
    key is 'z' (zoom in): mainLoop's first frame takes it (TAKE which=1 at 0x0041c0d7) and it changes the camera cache."""
    yield from SC._cov_b_menu(c)
    yield from c.checkpoint("menu")
    yield from c.click(130, 445)
    yield from c.wait_screen(SC.imgcmp.load(SC.GOLDEN / "location.png"), name="location")
    yield from c.checkpoint("location")
    c.move(*SC.COV_B_LOC["monterey"]); yield from c.sleep(500)
    c.act(600, k="post", msg=COVA_WM_CHAR, wp=ord("z"), lp=1, tag="char z, queued before the world load")
    yield from c.click(*SC.COV_B_LOC["monterey"])               # the click's own actions end at +500: the key lands after it
    yield from SC._cov_b_hud(c)
    yield from c.sleep(3000)
    yield from c.checkpoint("hud")
    for _ in range(4):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


SCENARIOS["cov_r_flic_input"] = {"env": {}, "phases": [], "lockstep": cov_r_flic_input}
