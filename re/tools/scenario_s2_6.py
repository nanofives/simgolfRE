"""Roadmap S2.6 scenario (log/roadmap/S2/6_notes.md): one checkpoint inside each 2D screen that no other pinned checkpoint
shows (re/roadmap/S2_screens.tsv, log/roadmap/S2/2_notes.md "Families no pinned checkpoint shows"). Registered into
scenario.SCENARIOS by the hook at the end of re/tools/scenario.py; the name starts with cov_r so scenario_refs' s36a family
runs it.

How each screen is reached (mainLoop addresses from shim/src/re/c3fy.cpp):
- title menu: Select A Theme (showThemePackSelect), Start New Game (selectDifficultyScreen), Play a Championship
  (pickAProScreen), each closed with Esc (ui_tour's steps);
- loading: the location click starts the course; showLoadingScreen runs before the first frame;
- report keys F1..F10 (second key switch 0x0041de11): F2 0x71 sets report byte [0x00569498] = 3 (showPlayerCommentsReport),
  F3 0x72 = 4 (showHistographScreen), F4 0x73 = 6 (showFinancialReport), F7 0x76 = 9 (showCourseReport(1)), F8 0x77 = 5
  (showShortcutsScreen), F9 0x78 = 0xa (showMembershipRoster), F10 0x79 runBulletinBoardWithSound; the report functions are
  called from the frame at 0x0041e095..0x0041e116 while the byte holds the value; F1 (SGA report), F5 (routing map) and F6
  (world map) are pinned elsewhere;
- hole stats: a hole key with the hole on screen stores -hole in [0x0053df54] (0x0041ddb4), and P6 calls
  showHoleStatsScreen (0x004198f0);
- radial menu: showMainRadialMenu runs every in-game frame (0x004197c4); its hover label is drawn once the same spot is
  hovered for more than 10 frames (0x00432de3);
- hire, preferences, system menu, best scores, skill points: the toolbar steps of ui_tour / cov_b_people;
- buy land: bit 0 of g_eventFlags567a1c (0x00567a1c) makes the frame show the land popup (0x0041e2a0..0x0041e325) and
  option 0 runs showBuyLandScreen (0x0041e331); the bit is set here by a deliberate write (patronEvent sets it in play);
- end of year: the year end runs when g_date (0x00834170) % 8192 == 0 (0x0041822b) and calls showEndOfYearScreen unless
  bit 0x4000000 of g_flags (0x0059e7b8) is set (0x004182e6); the date is moved to 0x30 frames before the year end by a
  deliberate write, in a career (the bit is clear)."""
import scenario as SC

SCENARIOS = SC.SCENARIOS
G_DATE = 0x00834170
G_EVENT_FLAGS = 0x00567A1C


def _report(c, name, vk, settle=3000):
    yield from c.key(vk); yield from c.sleep(settle)
    yield from c.checkpoint(name)
    yield from c.key(0x1B); yield from c.sleep(2000)


def cov_r_s2_screens(c):
    # ---- the boot's loading screen (mainLoop 0x0040f919 showLoadingScreen(0), then loadInterfaceArt(1))
    for t in BOOT_LOADING_T:
        yield from c.sleep(t - c.T)
        yield from c.checkpoint(f"loading_t{t}")
    yield from SC._cov_b_menu(c)
    yield from c.checkpoint("menu")
    # ---- title-menu screens
    yield from c.click(630, 425); yield from c.sleep(4000)      # Select A Theme
    yield from c.checkpoint("theme_packs")
    yield from c.key(0x1B); yield from c.sleep(3000)
    yield from c.click(560, 110); yield from c.sleep(4000)      # Start New Game
    yield from c.checkpoint("difficulty")
    yield from c.key(0x1B); yield from c.sleep(3000)
    yield from c.click(400, 540); yield from c.sleep(3000)      # Play a Championship
    yield from c.click(390, 125); yield from c.sleep(4000)      # Easy: the championship course list
    yield from c.checkpoint("champ_courses")
    yield from c.click(380, 122); yield from c.sleep(1000)      # first course in the list
    yield from c.click(630, 555); yield from c.sleep(4000)      # OK: the pro list (pickAProScreen)
    yield from c.checkpoint("pick_a_pro")
    yield from c.click(768, 557); yield from c.sleep(4000)      # one back arrow: the title menu
    yield from c.checkpoint("title_back")
    yield from SC._cov_b_menu(c, settle=1500)
    # ---- a career: Start New Game -> Easy -> Northeast (Monterey is unaffordable on this timeline)
    yield from c.click(560, 110); yield from c.sleep(3000)
    yield from c.click(390, 125)
    yield from c.wait_screen(SC.imgcmp.load(SC.GOLDEN / "location.png"), name="location")
    yield from c.click(*SC.COV_B_LOC["northeast"])
    yield from SC._cov_b_hud(c)
    yield from c.sleep(2000)
    yield from c.key(0x1B); yield from c.sleep(1500)            # welcome text
    yield from SC._cov_b_hole(c, HOLE_TEE, HOLE_GREEN)          # hole 1, opened
    yield from c.click(45, 470); yield from c.sleep(1200)       # the Build Course palette closed again
    yield from SC._cov_b_season(c, 4000)
    yield from c.checkpoint("course")
    # ---- radial menu hover label: the same spot for more than 10 frames
    c.move(45, 470); yield from c.sleep(2500)
    yield from c.checkpoint("radial_hover")
    c.move(400, 300); yield from c.sleep(1000)
    # ---- report keys
    for name, vk in (("rep_comments", 0x71), ("rep_histograph", 0x72), ("rep_financial", 0x73), ("rep_report1", 0x76),
                     ("rep_shortcuts", 0x77), ("rep_roster", 0x78)):
        yield from _report(c, name, vk)
    yield from c.key(0x79); yield from c.sleep(3000)            # F10: bulletin board
    yield from c.checkpoint("bulletin")
    yield from c.key(0x1B); yield from c.sleep(2000)
    # ---- hole stats: hole key 1 with hole 1 on screen
    yield from c.key(0x31, char="1"); yield from c.sleep(3000)
    yield from c.checkpoint("hole_stats")
    yield from c.key(0x1B); yield from c.sleep(2000)
    # ---- information menu: Best Scores
    yield from c.click(75, 550); yield from c.sleep(2000)
    yield from c.click(150, 536); yield from c.sleep(3000)
    yield from c.checkpoint("best_scores")
    yield from c.key(0x1B); yield from c.sleep(2000)
    # ---- wrench menu alone, then Edit Preferences
    yield from c.click(132, 580); yield from c.sleep(2000)
    yield from c.checkpoint("system_menu")
    yield from c.key(0x1B); yield from c.sleep(2000)
    yield from c.click(132, 580); yield from c.sleep(2000)
    yield from c.click(200, 487); yield from c.sleep(3000)
    yield from c.checkpoint("preferences")
    yield from c.click(*SC.COV_B_PREF_OK); yield from c.sleep(2000)
    # ---- hire an employee
    yield from c.click(175, 535); yield from c.sleep(2500)      # people palette
    yield from c.click(232, 537); yield from c.sleep(2500)      # employees tab
    yield from c.click(*HIRE_BUTTON); yield from c.sleep(3000)  # HIRE AN EMPLOYEE dialog
    yield from c.checkpoint("hire")
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.click(175, 535); yield from c.sleep(1500)      # palette closed
    yield from c.key(0x1B); yield from c.sleep(1500)
    # ---- end of year: the date 0x30 frames before the year end
    d = SC._cov_b_obs(c, "date", lambda: SC._cov_b_read_u32(c, G_DATE))
    c.write32(G_DATE, (d | 0x1FFF) - 0x30, tag="g_date -> 48 frames before the year end"); yield from c.sleep(8000)
    yield from c.checkpoint("end_of_year")
    yield from c.click(*EOY_BUTTON); yield from c.sleep(4000)
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.key(0x1B); yield from c.sleep(1500)
    # ---- skill points (pro tab): the dialog, its points-left popup, a practice round, cancelled
    yield from c.click(175, 535); yield from c.sleep(2500)
    yield from c.click(256, 511); yield from c.sleep(2500)
    yield from c.click(313, 520); yield from c.sleep(2500)
    yield from c.checkpoint("skill_points")
    for _ in range(3):
        yield from c.click(265, 97); yield from c.sleep(600)    # Power Hitter +1
    yield from c.click(541, 333); yield from c.sleep(2500)      # OK -> the points-left popup
    yield from c.checkpoint("skill_popup")
    yield from c.click(310, 286); yield from c.sleep(2000)      # "Whoops, my bad."
    yield from c.click(541, 333); yield from c.sleep(2500)
    yield from c.click(400, 263); yield from c.sleep(4000)      # "Yea, I don't need ..."
    for _ in range(6):
        yield from c.sleep(1000)
    yield from c.checkpoint("practice")
    for i, answer in enumerate(((380, 228), (360, 204))):       # wrench: Cancel match -> No, then Yes
        yield from c.click(132, 580); yield from c.sleep(2000)
        yield from c.click(200, 511); yield from c.sleep(2500)
        if i == 0:
            yield from c.checkpoint("cancel_prompt")
        yield from c.click(*answer); yield from c.sleep(3000)
    # the character editor (pro tab); its load control 8 (table 0x004c7b38 entry 8 = (48, 22)) would show the file list
    # in scrollTextWindow, but under the lockstep harness that click stops the clock (U-1250), so it is not pressed
    yield from c.click(272, 560); yield from c.sleep(3000)
    yield from c.checkpoint("editor")
    yield from c.click(753, 213); yield from c.sleep(3000)      # check mark: back to the course
    yield from SC._cov_b_season(c, 6000)
    yield from c.checkpoint("pre_land")
    # ---- buy land, last (the port has no showBuyLandScreen: re/port/commands_split.tsv): the event bit, popup, option 0
    c.write32(G_EVENT_FLAGS, 1, tag="g_eventFlags567a1c bit 0: the land popup"); yield from c.sleep(4000)
    yield from c.checkpoint("land_popup")
    yield from c.click(*LAND_YES); yield from c.sleep(4000)
    yield from c.checkpoint("buy_land")
    yield from c.click(400, 300, right=True); yield from c.sleep(3000)   # right click: back without a purchase
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from SC._cov_b_season(c, 6000)
    yield from c.checkpoint("end")


BOOT_LOADING_T = (200, 300)   # virtual ms of the boot checkpoints: both show the loading screen (probe4); not 100: the
# trace recorder writes the first poll's checkpoint name empty (6_notes.md "Trace")
HOLE_TEE, HOLE_GREEN = (220, 340), (520, 330)   # inside the owned centre tract: probe4 frames course and buy_land
HIRE_BUTTON = (270, 558)    # hitEmployee436b00 table 0x004c7af0 entry 0 (254, 542) + 0x10: code 0, [0x0059ca54] = 0, hireEmployee
LAND_YES = (380, 222)       # option 0 of the land popup ("Yup, ..."): measured on the probe3 frame land_popup
EOY_BUTTON = (576, 246)     # the end-of-year check mark (probe6 frame end_of_year); any left click ends its loop (0x0044e45c)

SCENARIOS["cov_r_s2_screens"] = {"env": {}, "phases": [], "lockstep": cov_r_s2_screens}
