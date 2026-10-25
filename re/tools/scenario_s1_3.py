"""Roadmap S1.3 scenarios (log/roadmap/S1/3_notes.md, U-1066 parts 2 and 3). Registered into scenario.SCENARIOS by the hook at
the end of re/tools/scenario.py; names start with cov_r so scenario_refs' s36a family runs them.

cov_r_theme_cancel: the title menu's Select A Theme screen (showThemePackSelect 0x004725b0), a theme row pressed, then Cancel
(hot button 1, hotspot (0x2ED, 0x22C): showThemePackSelect sets g_theme back to "Standard" at its exit kind 1).

cov_r_palette_hot: the Build Course palette's two fixed hotspots in a sandbox: hit-test code 0x15 (hotspot (0x105, 0x242), the
Undo button: clickSlotPanel 0x00433040 draws the undo help window and waits in pumpAndRedraw(0)) and 0x16 (hotspot (0xED,
0x20D), the wrench tab: [0x0055e924] = 1, the amenities page). showInfoCursorPanel 0x00433190 draws them with the palette
SpriteRefs 84 and 88 (0x00571b28, 0x00571bd8), the two cuts loadInterfaceArt writes at +0x4fb3 / +0x4fd4."""
import scenario as SC

SCENARIOS = SC.SCENARIOS
G_THEME = 0x00567328            # g_theme (SG_OBJECT g_theme, offset 0x0a6328 from 0x004c1000): a NUL-terminated name


def cov_r_theme_cancel(c):
    """Select A Theme -> a theme row -> Cancel (g_theme back to Standard) -> the row again and Load (the theme kept) -> a third
    visit: the row, then Cancel (g_theme back to Standard) -> Start New Game -> Impossible -> Northeast and a few seconds of
    play, whose frame records carry g_theme (a Cancel that keeps the theme shows there)."""
    yield from SC._cov_b_menu(c)
    yield from c.checkpoint("menu")
    before = SC._cov_b_obs(c, "theme_before", lambda: SC._cov_b_read_u32(c, G_THEME))
    yield from c.click(630, 425); yield from c.sleep(5000)      # Select A Theme
    yield from c.click(100, 109); yield from c.sleep(1500)      # row 0: g_theme = that row
    picked = SC._cov_b_obs(c, "theme_picked", lambda: SC._cov_b_read_u32(c, G_THEME))
    if c.run.mode == "record" and picked == before:
        raise RuntimeError(f"the theme row press left g_theme at 0x{before:08x}")
    yield from c.click(749, 556); yield from c.sleep(5000)      # Cancel
    after = SC._cov_b_obs(c, "theme_cancel", lambda: SC._cov_b_read_u32(c, G_THEME))
    if c.run.mode == "record" and after != before:
        raise RuntimeError(f"Cancel left g_theme at 0x{after:08x}, expected 0x{before:08x}")
    yield from c.checkpoint("cancelled")
    yield from c.click(630, 425); yield from c.sleep(5000)      # Select A Theme again
    yield from c.click(100, 109); yield from c.sleep(1500)      # the row again
    yield from c.click(615, 555); yield from c.sleep(5000)      # Load: the theme is kept
    yield from c.checkpoint("loaded")
    # a third visit: listThemes resets g_theme to Standard on entry, so only a row pressed in THIS visit makes Cancel visible
    yield from c.click(630, 425); yield from c.sleep(5000)
    yield from c.click(100, 109); yield from c.sleep(1500)
    picked2 = SC._cov_b_obs(c, "theme_picked2", lambda: SC._cov_b_read_u32(c, G_THEME))
    if c.run.mode == "record" and picked2 == before:
        raise RuntimeError(f"the third visit's row press left g_theme at 0x{before:08x}")
    yield from c.click(749, 556); yield from c.sleep(5000)
    after2 = SC._cov_b_obs(c, "theme_cancel2", lambda: SC._cov_b_read_u32(c, G_THEME))
    if c.run.mode == "record" and after2 != before:
        raise RuntimeError(f"the second Cancel left g_theme at 0x{after2:08x}, expected 0x{before:08x}")
    yield from c.checkpoint("cancelled2")
    # a sandbox ignores g_theme (control 1 and 2 of showThemePackSelect read GREEN on a sandbox run): a career reads it
    yield from c.click(560, 110); yield from c.sleep(4000)      # Start New Game
    yield from c.click(400, 476); yield from c.sleep(8000)      # Impossible
    yield from c.click(*SC.COV_B_LOC["northeast"])
    yield from SC._cov_b_hud(c)
    yield from c.sleep(4000)
    yield from c.checkpoint("career")
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from SC._cov_b_season(c, 6000)
    yield from c.checkpoint("end")


SCENARIOS["cov_r_theme_cancel"] = {"env": {}, "phases": [], "lockstep": cov_r_theme_cancel}


def cov_r_palette_hot(c):
    """Sandbox Monterey, Build Course palette open: a click on the Undo hotspot (code 0x15) and its help window closed by a
    key, a click on the wrench tab (code 0x16, amenities page), a hover over each, a tee placed on the first page, play."""
    yield from SC._cov_b_menu(c)
    yield from SC._cov_b_sandbox(c, "monterey")
    yield from c.checkpoint("sandbox")
    yield from c.click(45, 470); yield from c.sleep(1500)       # the Build Course palette
    c.move(261, 578); yield from c.sleep(1500)                  # hover: showInfoCursorPanel draws SpriteRef 84
    yield from c.checkpoint("hover_undo")
    yield from c.click(261, 578); yield from c.sleep(2000)      # Undo hotspot: the help window, pumpAndRedraw(0)
    yield from c.checkpoint("undo_help")
    yield from c.key(0x1B); yield from c.sleep(1500)
    c.move(237, 525); yield from c.sleep(1500)                  # hover: SpriteRef 88
    yield from c.checkpoint("hover_wrench")
    yield from c.click(237, 525); yield from c.sleep(1500)      # wrench tab: [0x0055e924] = 1
    yield from c.checkpoint("amenities")
    yield from c.click(261, 578); yield from c.sleep(1500)      # on the amenities page the click goes to clickAmenityPanel
    yield from c.key(0x1B); yield from c.sleep(1000)
    yield from c.click(237, 525); yield from c.sleep(1500)
    yield from c.click(45, 470); yield from c.sleep(1200)       # palette closed
    yield from SC._cov_b_hole(c, (300, 200), (520, 330))        # the palette's first page once more (tee, green, open)
    yield from c.click(45, 470); yield from c.sleep(1200)
    yield from c.click(261, 578); yield from c.sleep(2000)      # Undo with a hole built
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from SC._cov_b_season(c, 10000)
    yield from c.checkpoint("end")


SCENARIOS["cov_r_palette_hot"] = {"env": {}, "phases": [], "lockstep": cov_r_palette_hot}
