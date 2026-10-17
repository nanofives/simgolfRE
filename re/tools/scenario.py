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
               ("tournament", lambda g: play_tournament(g, 240))],
}


# ---- Gary Golf's shots (tournament) ----------------------------------------------------------------
# Manual p.25: click a shot-type icon, a white trajectory line follows the mouse, click to hit.
SHOT_PANEL = (330, 485, 800, 535)      # the five shot-type ovals (golden: tests/golden/shot_panel.png)
SHOT_STRAIGHT = (415, 508)
PLAY_AREA = (0, 70, 800, 470)          # excludes the leaderboard, gauges and the bottom HUD


def find_green(img):
    """Centroid (x, y) of the putting-green colour in the play area, or None.
    Green surface ~ (180, 240, 128); fairway ~ (105, 148, 78) (sampled 2026-10-02)."""
    import numpy as np
    a = np.asarray(img.convert("RGB"), dtype=np.int16)
    x0, y0, x1, y1 = PLAY_AREA
    sub = a[y0:y1, x0:x1]
    r, g, b = sub[..., 0], sub[..., 1], sub[..., 2]
    mask = (g > 215) & (r > 150) & (r < 205) & (b > 100) & (b < 150)
    if mask.sum() < 40:
        return None
    ys, xs = np.nonzero(mask)
    return int(xs.mean()) + x0, int(ys.mean()) + y0


def find_pin(img, near, radius=70):
    """Base of the flagstick (= the cup) near `near`.
    Sampled 2026-10-02: pennant R 165-231, G 16-123, B 16-99, hanging to the RIGHT of the pole; pole pixels
    pale/beige (e.g. (189,181,156), (181,173,165)); base where the pole meets the grass. None if not found."""
    import numpy as np
    a = np.asarray(img.convert("RGB"), dtype=np.int16)
    cx, cy = near
    x0, y0 = max(cx - radius, 0), max(cy - radius, PLAY_AREA[1])
    x1, y1 = min(cx + radius, 799), min(cy + radius, PLAY_AREA[3])
    sub = a[y0:y1, x0:x1]
    r, g, b = sub[..., 0], sub[..., 1], sub[..., 2]
    red = (r > 150) & (g < 130) & (b < 110) & (r - g > 60)
    if red.sum() < 4:
        return None
    ys, xs = np.nonzero(red)

    def pole(px):
        pr, pg, pb = (int(v) for v in px)
        return pr >= 150 and abs(pr - pg) < 45 and pb >= 100

    # Other reds exist nearby (flowers, the press box): accept the red pixel closest to the green that has
    # a pole run (> 8 px) going down from it in one of the columns just left of it.
    order = np.argsort((xs + x0 - cx) ** 2 + (ys + y0 - cy) ** 2)
    for k in order[:200]:
        fx, fy = int(xs[k]) + x0, int(ys[k]) + y0
        for x in (fx - 1, fx - 2):
            if x < 0:
                continue
            y, gap = fy, 0
            while y + 1 < y1 and gap <= 1:
                y += 1
                gap = 0 if pole(a[y, x]) else gap + 1
            bottom = y - gap
            if bottom - fy > 8:
                return x, bottom
    return None


def gary_turn(g):
    import imgcmp
    return imgcmp.matches(g.shot().crop(SHOT_PANEL), imgcmp.load(GOLDEN / "shot_panel.png"), 25)


def play_gary_shot(g, log=None):
    """One shot: straight shot at the green centroid (or screen centre if no green is visible)."""
    # Aim in the camera view that follows Gary. Centring the view on the hole first ('1') was tried and
    # failed: a target beyond the club's reach is ignored, so the shot never fires (2026-10-02).
    img = g.shot()
    green = find_green(img)
    zoomed = 0
    while green is None and zoomed < 3:   # green off-screen (camera follows the ball): zoom out ('X')
        g.key(0x58)
        zoomed += 1
        time.sleep(0.8)
        img = g.shot()
        green = find_green(img)
    target = (find_pin(img, green) if green else None) or green or (400, 260)
    g.click(*SHOT_STRAIGHT)
    time.sleep(0.8)
    g.move(*target)
    time.sleep(0.6)
    g.click(*target)
    if log is not None:
        log.append((target, zoomed))
    for _ in range(zoomed):               # restore the zoom level for the next detection
        g.key(0x5A)
        time.sleep(0.3)
    return target


RESULTS_HEADER = (200, 0, 600, 40)     # "TOURNAMENT RESULTS" (golden: tests/golden/tournament_results.png)
RESULTS_OK = (755, 140)                # check mark that closes the results panel


def tournament_over(img):
    import imgcmp
    return imgcmp.matches(img.crop(RESULTS_HEADER), imgcmp.load(GOLDEN / "tournament_results.png"), 25)


def play_tournament(g, seconds=360, max_shots=80):
    """Play the tournament to the end: take Gary's shot whenever the shot panel is up, clear popups with
    Esc otherwise, stop on TOURNAMENT RESULTS (closed with its check mark).
    Race found 2026-10-02: Esc pressed while the results panel appears quits to the main menu, so the round
    ended but the results were never seen (2 of 4 rounds "failed" that way). The results check now runs
    right before every Esc, and landing on the main menu after Esc counts as finished (results_seen False).
    Returns {"shots": [...], "finished": bool, "results_seen": bool}."""
    import imgcmp
    menu = imgcmp.load(GOLDEN / "main_menu.png")
    shots = []
    t0 = time.time()
    while time.time() - t0 < seconds and len(shots) < max_shots:
        img = g.shot()
        if tournament_over(img):
            g.shot(ROOT / "log" / "tournament_results_last.png")
            g.click(*RESULTS_OK)
            time.sleep(2)
            return {"shots": shots, "finished": True, "results_seen": True}
        if gary_turn(g):
            play_gary_shot(g, shots)
            time.sleep(2)
            continue
        time.sleep(0.7)                    # the results panel animates in: look twice before pressing Esc
        if tournament_over(g.shot()):
            continue
        g.key(0x1B)
        time.sleep(1.5)
        if imgcmp.matches(g.shot(), menu):
            return {"shots": shots, "finished": True, "results_seen": False}
        if not g.alive:
            raise RuntimeError(f"game died during tournament: {g.detach_reason}")
    return {"shots": shots, "finished": False, "results_seen": False}


# ---- Golfer panel ------------------------------------------------------------------------------------
HUD_BOXES = [(0, 0, 240, 95), (640, 0, 800, 145), (240, 0, 640, 30)]


def find_golfer_labels(img, min_px=12):
    """Golfer name labels: near-white text (R,G,B > 235) in the play area, grouped into boxes.
    Returns [(cx, cy_bottom), ...] largest first; the golfer sprite stands just below its label."""
    import numpy as np
    a = np.asarray(img.convert("RGB"), dtype=np.int16)
    x0, y0, x1, y1 = PLAY_AREA
    sub = a[y0:y1, x0:x1]
    m = (sub[..., 0] > 235) & (sub[..., 1] > 235) & (sub[..., 2] > 235)
    for hx0, hy0, hx1, hy1 in HUD_BOXES:           # course-name box, gauges, message banner
        m[max(hy0 - y0, 0):max(hy1 - y0, 0), hx0 - x0:hx1 - x0] = False
    ys, xs = np.nonzero(m)
    boxes = []
    used = np.zeros(len(xs), bool)
    for i in range(len(xs)):
        if used[i]:
            continue
        near = (np.abs(xs - xs[i]) < 40) & (np.abs(ys - ys[i]) < 8) & ~used
        used |= near
        if near.sum() >= min_px:
            bx, by = xs[near], ys[near]
            boxes.append((int(near.sum()), int(bx.mean()) + x0, int(by.max()) + y0))
    boxes.sort(reverse=True)
    return [(cx, cy) for _, cx, cy in boxes]


GOLFER_PANEL_FN = 0x0045C560          # cdecl (golfer index, ?) — golfer info panel (C1 GolferPanelEject)
LANDMARK_NOTICE_FN = 0x004722C0       # cdecl (golfer index, story id) — landmark-available notice
STORY_ID = 0x00579568                 # short at + idx*0x100 (golfer +0xb0, -1 = none): the 2nd argument at the
                                      # only call site 0x004667e1
STORY_ADVANCE_FN = 0x00466370         # cdecl (golfer, force) — advances a pair story one chapter (UNCERTAINTIES U-0003)


def events_script(g):
    """The events.js agent (loaded by scenarios that declare it in "js")."""
    for sc in g.scripts:
        try:
            sc.exports_sync.done()
            return sc.exports_sync
        except Exception:
            continue
    raise RuntimeError("events.js not loaded")


def open_golfer_panel(g, tries=10):
    """Organic: click golfers under their name labels until the info panel opens (0x0045c560 gets called).
    Other white text exists (hole labels, messages), so every label is tried at a few offsets below it."""
    ev = events_script(g)
    ev.watch(GOLFER_PANEL_FN)
    for _ in range(tries):
        for x, y in find_golfer_labels(g.shot())[:6]:
            for dy in (14, 8, 22):
                g.click(x, y + dy)
                time.sleep(1.0)
                if ev.calls(GOLFER_PANEL_FN):
                    g.shot(ROOT / "log" / "golfer_panel_last.png")
                    return ev.calls(GOLFER_PANEL_FN)[-1][0]
        g.key(0x1B)
        time.sleep(2)
    raise RuntimeError("no golfer panel opened")


def inject_landmark_notice(g, golfer):
    """NOT organic: call the landmark-available notice on the game thread for a golfer that is on the course,
    with the story-id argument the game itself would pass (short at STORY_ID + golfer*0x100).
    The organic path is a pair story reaching chapter 4 (golfer_stories scenario, UNCERTAINTIES U-0003)."""
    ev = events_script(g)
    story = ev.read_s16(STORY_ID + golfer * 0x100)
    ev.inject(LANDMARK_NOTICE_FN, golfer, story)
    for _ in range(20):
        time.sleep(0.25)
        if ev.done():
            break
    time.sleep(2)
    g.shot(ROOT / "log" / "landmark_notice_last.png")
    result = ev.done()
    print("landmark injection:", result, flush=True)
    return result


def _golfer_events(g):
    golfer = open_golfer_panel(g)
    g.click(592, 258)                 # close the panel (check mark)
    time.sleep(1)
    result = inject_landmark_notice(g, golfer)
    g.extra = {"golfer": golfer, "inject": result}


SCENARIOS["golfer_events"] = {
    "env": {"SIMGOLF_TIMEWARP": "8"},
    "js": ["events.js"],
    # Functions hooked by events.js must not also get an INT3 (both patch the first byte); the census
    # records them from the watch() evidence instead, in the phase named here.
    "watched": {GOLFER_PANEL_FN: "golfer_panel"},
    "phases": [("menu", lambda g: time.sleep(3)),
               ("sandbox", lambda g: to_sandbox(g)),
               ("build_hole", lambda g: build_first_hole(g)),
               ("season", lambda g: run_season(g, 25)),
               ("golfer_panel", lambda g: (open_golfer_panel(g), g.click(592, 258), time.sleep(1))),
               ("landmark_inject", lambda g: inject_landmark_notice(g, events_script(g).calls(GOLFER_PANEL_FN)[-1][0]))],
}


# ---- Golfer pair stories (organic path toward the landmark, U-0003) -------------------------------------
# FUN_00466370 advances a story only when the golfer's current hole (char +0x21) is >= 2 (or byte +0x22 != 0),
# so on the 1-hole course stories never move. A multi-hole course moves them within one game year at x32. Terrain is random per run, so some of the
# FOUR_HOLES placements can be refused (water, trees); one extra hole is enough to advance a story.

def build_next_hole(g, tee, green):
    """With the Build Course palette still open (build_first_hole leaves it open): add tee + green, open it."""
    g.click(*TOOL_TEE); time.sleep(0.8)
    g.click(*tee); time.sleep(1.2)
    g.click(*TOOL_GREEN); time.sleep(0.8)
    g.click(*green); time.sleep(2)
    g.click(300, 350, right=True); time.sleep(1)
    g.key(VK_H); time.sleep(3)


FOUR_HOLES = [((570, 380), (330, 280)), ((370, 300), (620, 250)), ((650, 280), (450, 180))]
# Extra spots tried when the random terrain refused some of FOUR_HOLES (water, trees, too close to a green).
FALLBACK_HOLES = [((150, 250), (280, 300)), ((600, 200), (720, 280)), ((450, 140), (600, 110)),
                  ((200, 380), (350, 420)), ((540, 430), (680, 380)), ((100, 160), (230, 130))]
HOLES_PLUS1 = 0x005685F0                 # short: open holes + 1 (Ghidra: g_holes_plus1; 6 on the 5-hole save)


def open_holes(g):
    return events_script(g).read_s16(HOLES_PLUS1) - 1


def build_four_holes(g, want=2):
    """Builds up to 4 holes; on terrain that refuses them, keeps trying FALLBACK_HOLES until at least `want`
    are open (story advances need a hole >= 2). Returns the number of open holes."""
    build_first_hole(g)
    for tee, green in FOUR_HOLES:
        build_next_hole(g, tee, green)
    for tee, green in FALLBACK_HOLES:
        if open_holes(g) >= max(want, 4):
            break
        build_next_hole(g, tee, green)
    return open_holes(g)


def story_season(g, seconds=150):
    """Run the season until FUN_00466370 returns non-zero (a chapter advanced: it returns 0 on every gate,
    log/decomp_466370.c); records the advancing calls and the best story stage (short +0xb2) in g.extra."""
    ev = events_script(g)
    ev.watch(STORY_ADVANCE_FN)
    t0, best, advanced = time.time(), 0, []
    while time.time() - t0 < seconds:
        run_season(g, 10)
        best = max([best] + [s[3] for s in ev.stories()])
        advanced = [c for c in ev.calls(STORY_ADVANCE_FN) if c[2]]
        if advanced:
            break
    g.extra = {"advanced": advanced, "best_stage": best, "calls": len(ev.calls(STORY_ADVANCE_FN))}
    print("stories:", g.extra, flush=True)
    if not advanced:
        raise RuntimeError(f"no story chapter advanced: {g.extra}")


SCENARIOS["golfer_stories"] = {
    "env": {"SIMGOLF_TIMEWARP": "32"},
    "js": ["events.js"],
    "watched": {STORY_ADVANCE_FN: "season"},
    "phases": [("menu", lambda g: time.sleep(3)),
               ("sandbox", lambda g: to_sandbox(g)),
               ("build_holes", lambda g: build_four_holes(g)),
               ("season", lambda g: story_season(g))],
}


# ---- Saved-game fixtures ----------------------------------------------------------------------------
# The game saves to "saved games\" under its working directory (original\); the installer creates that folder
# and the portable install did not, so saves failed silently until it existed (2026-10-03).
SAVED_GAMES = ROOT / "original" / "saved games"
STORIES_FIXTURE = ROOT / "tests" / "fixtures" / "stories" / "stories5.sve"


def load_saved_game(g, fixture):
    """main menu -> Continue Saved Game -> pick `fixture` (copied into saved games) -> OK -> in game.
    The list shows the game's own autosaves (&AutoSave*.sve) first, then the other files by name."""
    SAVED_GAMES.mkdir(exist_ok=True)
    shutil.copy2(fixture, SAVED_GAMES / fixture.name)
    others = sorted(p.name.lower() for p in SAVED_GAMES.glob("*.sve") if not p.name.startswith("&"))
    row = len(list(SAVED_GAMES.glob("&*.sve"))) + others.index(fixture.name.lower())
    imgcmp.wait_for(g, imgcmp.load(GOLDEN / "main_menu.png"), timeout=60)
    g.click(170, 80); time.sleep(3)
    g.click(400, 121 + 16 * row); time.sleep(1)
    g.click(632, 553); time.sleep(8)


def view_stories_course(g):
    """After loading the stories fixture the camera starts below-right of the course; 2x Up + 1x Left (short
    presses, ~140 px each) brings all 5 holes into the rect FUN_004289e0 requires (x 0x32..0x2ee, y 0x32..0x1c2)."""
    for _ in range(4):
        g.key(0x1B); time.sleep(0.5)
    for vk in (0x26, 0x26, 0x25):
        g.key(vk, hold=0.05); time.sleep(1)


SCENARIOS["stories_fixture"] = {
    "env": {"SIMGOLF_TIMEWARP": "32"},
    "js": ["events.js"],
    "watched": {STORY_ADVANCE_FN: "season"},
    "phases": [("load", lambda g: (load_saved_game(g, STORIES_FIXTURE), view_stories_course(g))),
               ("season", lambda g: story_season(g))],
}


# 9-hole fixture on Scotland (Harold's Keep): 5 holes on the left field, 4 on the right, 35-103 yards.
# Built at normal zoom. Zooming out does NOT help: in a 25 min x32 run zoomed out no story advanced past stage 1
# (the rect test in FUN_004289e0 uses positions that leave the left holes outside when zoomed out). After loading,
# the camera sits ~100 px left of the build view: the course spans x ~20-700, so only the tees of holes 1-5
# fall left of the rect (x < 0x32). Arrow keys move ~250 px per press and edge scrolling did nothing, so this
# is the best view found. Purpose: room for 4 chapter advances in one visit (U-0003).
STORIES9_FIXTURE = ROOT / "tests" / "fixtures" / "stories" / "stories9.sve"


CAMERA_TILE = (0x4c2ba0, 0x4c2ba4)       # camera centre tile x, y (world->screen FUN_0042fb90 reads them)


def view_stories9_course(g):
    """Close popups and the palette, then put the camera on tile (24, 24): the course is ~760 px wide at normal
    zoom, wider than the 0x32..0x2ee rect; (24, 24) leaves ~40 px out on the left and ~20 on the right.
    Arrow keys move the camera 4 tiles per press (~250 px), so the 1-tile step is a direct write (view only)."""
    for _ in range(4):
        g.key(0x1B); time.sleep(0.5)
    g.click(*GOLF_ICON); time.sleep(1)    # the save has the Build Course palette open: close it
    events_script(g).set_camera(24, 24); time.sleep(1)


SCENARIOS["stories9_fixture"] = {
    "env": {"SIMGOLF_TIMEWARP": "32"},
    "js": ["events.js"],
    "watched": {STORY_ADVANCE_FN: "season"},
    "phases": [("load", lambda g: (load_saved_game(g, STORIES9_FIXTURE), view_stories9_course(g))),
               ("season", lambda g: story_season(g))],
}


# 5-hole fixture built to keep golfer moods up (2026-10-04, Ocean's Edge MC): every hole has a fairway painted
# between tee and green, so shots stay off the rough (thought 2 'Darn, I'm in the rough!', -1) and the
# rough-to-bad-lie complaint (thought 8, -2) does not fire. Measured before saving: moods 2..10 over 150 s at x32
# and 1 golfer type banned, where the tee+green-only courses lost 8-12 types in 200 s.
# The holes are short (~90 yd) and get the hole flag 8 ('too easy', FUN_0042dea0), which costs -1 per under-par
# score (thought 0x13 -> 0x17 in FUN_00467a00), but that stays below the fairway gain. Not committed (game-derived).
MOOD5_FIXTURE = ROOT / "tests" / "fixtures" / "stories" / "mood5.sve"


def view_mood5_course(g):
    """Close popups and put the camera back on the tile it had when the save was made (28, 15)."""
    for _ in range(4):
        g.key(0x1B); time.sleep(0.5)
    events_script(g).set_camera(28, 15); time.sleep(1)


SCENARIOS["mood5_fixture"] = {
    "env": {"SIMGOLF_TIMEWARP": "32"},
    "js": ["events.js"],
    "watched": {STORY_ADVANCE_FN: "season"},
    "phases": [("load", lambda g: (load_saved_game(g, MOOD5_FIXTURE), view_mood5_course(g))),
               ("season", lambda g: story_season(g))],
}


# ---- UI tour (s36b, round 36: C4 by the scenario route) ------------------------------------------------
# Frame-locked only: "lockstep" is a scenario_ab.py Ctx script (virtual clock, recorded once, replayed in every arm);
# log/c3/s36b_run.py registers it into scenario_ab.py. Notes: log/c3/s36b_notes.txt.
def ui_tour_lockstep(c):
    yield from c.wait_screen(imgcmp.load(GOLDEN / "main_menu.png"), name="main_menu")
    yield from c.sleep(3000)
    yield from c.checkpoint("menu")
    yield from c.click(630, 425); yield from c.sleep(4000)      # Select A Theme
    yield from c.key(0x1B); yield from c.sleep(3000)
    yield from c.click(170, 80); yield from c.sleep(4000)       # Continue Saved Game
    yield from c.key(0x1B); yield from c.sleep(3000)
    yield from c.click(560, 110); yield from c.sleep(4000)      # Start New Game
    yield from c.key(0x1B); yield from c.sleep(3000)
    yield from c.click(400, 540); yield from c.sleep(4000)      # Play a Championship
    yield from c.key(0x1B); yield from c.sleep(3000)
    yield from c.checkpoint("menu_done")
    yield from c.click(560, 110); yield from c.sleep(3000)      # Start New Game -> Easy -> Monterey
    yield from c.click(390, 125)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "location.png"), name="location")
    yield from c.click(270, 32)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "ingame_hud_dial.png"), crop=DIAL, threshold=30, name="hud")
    yield from c.sleep(6000)
    yield from c.checkpoint("game")
    yield from c.key(0x1B); yield from c.sleep(1500)             # welcome text
    yield from c.click(45, 470); yield from c.sleep(1500)        # Build Course palette
    yield from c.click(300, 527); yield from c.sleep(1000)       # tee tool
    yield from c.click(110, 290); yield from c.sleep(1500)
    yield from c.click(365, 527); yield from c.sleep(1000)       # green tool
    yield from c.click(330, 400); yield from c.sleep(2500)
    yield from c.click(300, 350, right=True); yield from c.sleep(1000)
    yield from c.key(0x48); yield from c.sleep(3000)            # H: open the hole
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.click(118, 495); yield from c.sleep(2500)       # buildings palette
    yield from c.click(390, 530); yield from c.sleep(1500)       # first building tool
    yield from c.click(230, 300); yield from c.sleep(2500)       # place it
    yield from c.click(300, 350, right=True); yield from c.sleep(1000)
    yield from c.click(118, 495); yield from c.sleep(2500)       # buildings palette again: tooltips
    for x in range(390, 760, 60):
        for y in (527, 570):
            c.move(x, y); yield from c.sleep(1000)
    yield from c.click(118, 495); yield from c.sleep(1500)       # close it
    # people palette: golfer list -> golfer panel, employees, character editor
    yield from c.click(175, 535); yield from c.sleep(2500)
    yield from c.click(360, 507); yield from c.sleep(3000)       # first golfer in the list
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.click(232, 537); yield from c.sleep(2500)       # employees tab
    for y in (530, 575):
        for x in (368, 432, 496, 560):
            c.move(x, y); yield from c.sleep(1000)              # tooltips of the eight employee kinds
    for x in (650, 705, 760):
        c.move(x, 513); yield from c.sleep(1000)
    yield from c.click(368, 530); yield from c.sleep(3000)       # HIRE AN EMPLOYEE dialog
    yield from c.click(330, 318); yield from c.sleep(3000)       # Groundskeeper: 200 per week
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.click(368, 530); yield from c.sleep(3000)       # the hired employee's slot
    for x in (650, 705, 760):
        c.move(x, 513); yield from c.sleep(1000)
    yield from c.click(760, 513); yield from c.sleep(2500)       # third action icon
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.click(650, 513); yield from c.sleep(2000)       # first action icon
    yield from c.click(250, 380); yield from c.sleep(3000)
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.click(272, 560); yield from c.sleep(3000)       # character editor
    for y in (29, 80, 131, 180, 229):                            # its five appearance tabs
        yield from c.click(453, y); yield from c.sleep(1500)
    yield from c.click(753, 213); yield from c.sleep(3000)       # check mark: back to the course
    yield from c.checkpoint("people")
    # report screens F1..F10 (mainLoop's key switch 0x0041de11, report byte 0x00569498)
    for vk in range(0x70, 0x7A):
        yield from c.key(vk); yield from c.sleep(3000)
        if vk == 0x75:                                           # F6 world map: Esc does not close it
            c.move(400, 30); yield from c.sleep(1500)            # hover Monterey (tooltip)
            yield from c.click(770, 430); yield from c.sleep(3000)   # load browser
            yield from c.key(0x1B); yield from c.sleep(2000)
            yield from c.click(748, 488); yield from c.sleep(3000)   # save-name box
            for vk2 in (0x41, 0x42):
                yield from c.key(vk2); yield from c.sleep(300)
            yield from c.key(0x08, char="\b"); yield from c.sleep(300)
            yield from c.key(0x1B); yield from c.sleep(2000)
            yield from c.click(768, 557); yield from c.sleep(3000)   # back arrow
        else:
            yield from c.key(0x1B); yield from c.sleep(2000)
    yield from c.checkpoint("reports")
    # information menu (info button): Best Scores, Top 10 Designers, Repeat Last Message
    for y in (536, 560, 296):
        yield from c.click(75, 550); yield from c.sleep(2000)
        yield from c.click(150, y); yield from c.sleep(3000)
        yield from c.key(0x1B); yield from c.sleep(2000)
    # wrench menu: Edit Preferences, Rename Your Course, Save the Current Game, Load a Previous Game (all cancelled)
    for y in (487, 463, 367, 391):
        yield from c.click(132, 580); yield from c.sleep(2000)
        yield from c.click(200, y); yield from c.sleep(3000)
        if y == 487:                                             # preferences: one box off and on, then its check
            yield from c.click(236, 342); yield from c.sleep(800)
            yield from c.click(236, 342); yield from c.sleep(800)
            yield from c.click(560, 368); yield from c.sleep(2000)
            continue
        if y == 463:                                             # rename box: type, delete
            for vk2 in (0x43, 0x44):
                yield from c.key(vk2); yield from c.sleep(300)
            yield from c.key(0x08, char=""); yield from c.sleep(300)
        yield from c.key(0x1B); yield from c.sleep(2000)
    yield from c.checkpoint("menus")
    yield from c.click(108, 568); yield from c.sleep(2000)       # pause
    yield from c.click(108, 568); yield from c.sleep(2000)       # unpause
    yield from c.key(0x58); yield from c.sleep(1500)             # X: zoom out
    yield from c.key(0x5A); yield from c.sleep(1500)             # Z: zoom in
    # pro tab: skill points dialog and its "points left" popup (both answers); leaving it starts a practice round
    yield from c.click(175, 535); yield from c.sleep(2500)
    yield from c.click(256, 511); yield from c.sleep(2500)
    yield from c.click(313, 520); yield from c.sleep(2500)
    for _ in range(3):
        yield from c.click(265, 97); yield from c.sleep(600)     # Power Hitter +1
    yield from c.click(541, 333); yield from c.sleep(2500)       # OK -> popup
    yield from c.click(310, 286); yield from c.sleep(2000)       # "Whoops, my bad."
    yield from c.click(541, 333); yield from c.sleep(2500)       # OK again
    yield from c.click(400, 263); yield from c.sleep(4000)       # "Yea, I don't need ..."
    for _ in range(10):
        yield from c.sleep(1000)
    yield from c.checkpoint("practice")
    # Gary's turn in the practice round: straight shot at the green (found on the RECORD frame, replayed from the schedule)
    yield from c.key(0x1B); yield from c.sleep(1500)
    target = _ui_tour_point(c, "gary_target", lambda img: find_green(img) or (400, 260))
    yield from c.click(*SHOT_STRAIGHT); yield from c.sleep(800)
    c.move(*target); yield from c.sleep(600)
    yield from c.click(*target)
    for _ in range(12):
        yield from c.sleep(1000)
    yield from c.checkpoint("shot")
    for answer in ((380, 228), (360, 204)):                      # wrench: Cancel match/tournament -> No, then Yes
        yield from c.click(132, 580); yield from c.sleep(2000)
        yield from c.click(200, 511); yield from c.sleep(2500)
        yield from c.click(*answer); yield from c.sleep(3000)
    for _ in range(8):
        yield from c.sleep(1000)
    yield from c.checkpoint("cancelled")
    # wrench: Save Gary Golf / Save Course for Championship: the file exists in Themes\Championship, so each asks
    # "already exists! Overwrite / Cancel"; Esc cancels (nothing is written)
    for y in (439, 415):
        yield from c.click(132, 580); yield from c.sleep(2000)
        yield from c.click(200, y); yield from c.sleep(3000)
        yield from c.key(0x1B); yield from c.sleep(2500)
        yield from c.key(0x1B); yield from c.sleep(2000)
    for _ in range(5):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


def _ui_tour_point(c, name, find):
    """A screen position measured on the RECORD run's frame and kept in the schedule, so every replay clicks the same
    point without a screenshot."""
    pts = c.run.schedule.setdefault("points", {})
    key = f"{c.wait_index}:{name}"
    c.wait_index += 1
    if c.run.mode != "replay":
        x, y = find(c.run.g.shot())
        pts[key] = [int(x), int(y)]
    return tuple(pts[key])


SCENARIOS["ui_tour"] = {"env": {}, "phases": [], "lockstep": ui_tour_lockstep}   # no real-time (Game) form


# ---- Coverage B of mainLoop 0x0040f5c0 (c3fyb, 2026-10-16) ------------------------------------------------------------
# Frame-locked Ctx scripts (scenario_ab.py's lockstep form) that drive the mainLoop blocks outside the keyboard-command
# dispatch, the mouse / tool actions and the quit path: build tools, terrain edit, careers on every course type that
# runs, date / month / year transitions, title-menu branches, 18 holes, shots of every type, decorations, preferences.
# Runner: log/c3/c3fyb_run.py (instances/c3fyb*, outputs log/c3/alt/c3fyb/<scenario>/); notes log/c3/c3fyb_notes.txt.
# Every c.write32 is a deliberate state injection (each one is listed in the notes with the blocks it opens).

COV_B_DATE = 0x00834170
COV_B_FLAGS = 0x0059E7B8

# Build Course palette slots (golf icon (45, 470)); measured on the sandbox frame 2026-10-16.
COV_B_GOLF_TOP = [(300, 527), (365, 527), (430, 527), (490, 527), (550, 527), (612, 527), (675, 527), (735, 512), (770, 527)]
COV_B_GOLF_BOTTOM = [(330, 570), (395, 570), (458, 570), (520, 570), (582, 570), (645, 570), (705, 562), (770, 565)]
# Buildings palette (icon (118, 495)); ui_tour's hover grid.
COV_B_BLDG = [(x, y) for y in (527, 570) for x in range(390, 760, 60)]
# Location-screen icons (the round button left of each label)
COV_B_LOC = {"monterey": (270, 32), "ireland": (487, 25), "scotland": (527, 80), "phoenix": (437, 240),
       "hawaii": (457, 292), "oahu": (475, 350), "northeast": (487, 472), "wales": (565, 133),
       "sandiego": (330, 80), "rocky": (373, 133), "lasvegas": (418, 185), "spain": (600, 185),
       "florida": (622, 235), "jamaica": (645, 292), "novascotia": (480, 410), "carolina": (487, 532)}
COV_B_CASH = 0x00571FD4


def _cov_b_menu(c, settle=3000):
    yield from c.wait_screen(imgcmp.load(GOLDEN / "main_menu.png"), name="main_menu")
    yield from c.sleep(settle)


def _cov_b_hud(c, name="hud"):
    yield from c.wait_screen(imgcmp.load(GOLDEN / "ingame_hud_dial.png"), crop=DIAL, threshold=30, name=name)


def _cov_b_sandbox(c, loc="monterey"):
    yield from c.click(130, 445)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "location.png"), name="location")
    yield from c.click(*COV_B_LOC[loc])
    yield from _cov_b_hud(c)
    yield from c.sleep(2000)
    yield from c.key(0x1B); yield from c.sleep(1500)          # welcome text


def _cov_b_views(c):
    """Every camera rotation (PgUp +2, Home -2: report-key switch 0x004219b0) and zoom level ('x' out, 'z' in)."""
    for _ in range(4):
        yield from c.key(0x21); yield from c.sleep(1500)
    yield from c.key(0x24); yield from c.sleep(1500)
    yield from c.key(0x21); yield from c.sleep(1000)
    for _ in range(2):
        yield from c.key(0x58); yield from c.sleep(1500)
    for _ in range(2):
        yield from c.key(0x5A); yield from c.sleep(1500)


def _cov_b_tool(c, slot, spots, place=True):
    """Pick a palette slot, hover it over `spots` (placement previews), place once at the first spot, drop it."""
    yield from c.click(*slot); yield from c.sleep(800)
    for x, y in spots:
        c.move(x, y); yield from c.sleep(700)
    if place:
        yield from c.click(*spots[0]); yield from c.sleep(1200)
    yield from c.click(spots[-1][0], spots[-1][1], right=True); yield from c.sleep(800)


def _cov_b_golf_palette(c, area):
    yield from c.click(45, 470); yield from c.sleep(1500)
    for i, slot in enumerate(COV_B_GOLF_TOP + COV_B_GOLF_BOTTOM):
        x0, y0 = area[i % len(area)]
        yield from _cov_b_tool(c, slot, [(x0, y0), (x0 + 40, y0 + 20), (x0 - 30, y0 + 30)])
    yield from c.click(45, 470); yield from c.sleep(1200)


def _cov_b_bldg_palette(c, area):
    yield from c.click(118, 495); yield from c.sleep(1500)
    for i, slot in enumerate(COV_B_BLDG):
        x0, y0 = area[i % len(area)]
        yield from _cov_b_tool(c, slot, [(x0, y0), (x0 + 30, y0 - 20), (x0 - 40, y0 + 10)])
        yield from c.click(118, 495); yield from c.sleep(1200)    # placing closes nothing; reopen keeps it open
        yield from c.click(118, 495); yield from c.sleep(1200)
    yield from c.click(118, 495); yield from c.sleep(1200)


def _cov_b_hole(c, tee, green):
    yield from c.click(45, 470); yield from c.sleep(1200)
    yield from c.click(300, 527); yield from c.sleep(800)
    yield from c.click(*tee); yield from c.sleep(1200)
    yield from c.click(365, 527); yield from c.sleep(800)
    yield from c.click(*green); yield from c.sleep(2000)
    yield from c.click(300, 350, right=True); yield from c.sleep(1000)
    yield from c.key(0x48); yield from c.sleep(3000)
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.click(45, 470); yield from c.sleep(1200)


def _cov_b_edit(c):
    """Terrain edit mode ('e': [0x567afc] = 1, [0x55e928] = 1 -> grid overlay bit 2 of [0x59e7b8]), its three cursor
    modes (Tab: [0x542f20] + 1), raise '=' / lower '-' at a few cursor tiles, then 'e' again."""
    yield from c.key(0x45); yield from c.sleep(1500)
    for m in range(3):
        for (x, y) in ((400, 300), (460, 260)):
            c.move(x, y); yield from c.sleep(800)
            yield from c.key(0xBB, char="="); yield from c.sleep(600)
            yield from c.key(0xBB, char="="); yield from c.sleep(600)
            c.move(x - 60, y + 40); yield from c.sleep(600)
            yield from c.key(0xBD, char="-"); yield from c.sleep(600)
        yield from c.key(0x09, char="\t"); yield from c.sleep(1000)
    yield from c.key(0x45); yield from c.sleep(1500)
    yield from c.key(0x45); yield from c.sleep(1500)
    yield from c.key(0x1B); yield from c.sleep(1500)


def _cov_b_toggles(c):
    """'N' xors [0x5a5a00] bit 1 (0x0041cc7f), 'T' xors [0x59e7b8] 0x400 (0x0041d02c), 'P' xors bit 4 (0x0041d020:
    the P5 tick skip)."""
    for ch, vk in (("N", 0x4E), ("T", 0x54)):
        yield from c.key(vk, char=ch); yield from c.sleep(3000)
    yield from c.key(0x50, char="P"); yield from c.sleep(3000)
    yield from c.key(0x50, char="P"); yield from c.sleep(1000)
    for ch, vk in (("N", 0x4E), ("T", 0x54)):
        yield from c.key(vk, char=ch); yield from c.sleep(1500)


def _cov_b_season(c, ms, esc_every=2000):
    t = 0
    while t < ms:
        yield from c.key(0x1B)
        yield from c.sleep(esc_every - 200)
        t += esc_every


# ---------------------------------------------------------------- scenarios
def cov_b_build(c):
    """Sandbox Monterey: every rotation and zoom, every Build Course and Buildings palette slot (hover + place), a hole,
    terrain edit mode with its three cursor modes, the N / T / P toggles, then 60 virtual s of play."""
    yield from _cov_b_menu(c)
    yield from _cov_b_sandbox(c, "monterey")
    yield from c.checkpoint("sandbox")
    yield from _cov_b_views(c)
    yield from c.checkpoint("views")
    yield from _cov_b_hole(c, (300, 200), (520, 330))
    yield from c.checkpoint("hole")
    yield from _cov_b_golf_palette(c, [(420, 250), (560, 220), (360, 330), (600, 300)])
    yield from c.checkpoint("golf_palette")
    yield from _cov_b_bldg_palette(c, [(250, 300), (450, 380), (600, 250)])
    yield from c.checkpoint("bldg_palette")
    yield from _cov_b_edit(c)
    yield from c.checkpoint("edit")
    yield from _cov_b_toggles(c)
    yield from _cov_b_season(c, 60000)
    yield from c.checkpoint("end")


def cov_b_career(c):
    """Career (Start New Game -> Easy -> Phoenix, $70,000): a hole, play, then date injections into the tournament offer
    (date % 8192 == 0x1000, 0x00417f8a) and the year end (date % 8192 == 0, 0x004182ee showEndOfYearScreen) and into
    year 2 (magazine block 0x0040ff4d needs date > 0x2000)."""
    yield from _cov_b_menu(c)
    yield from c.click(560, 110); yield from c.sleep(3000)      # Start New Game
    yield from c.click(390, 125); yield from c.sleep(8000)      # Easy
    yield from c.click(*COV_B_LOC["phoenix"])
    yield from _cov_b_hud(c)
    yield from c.sleep(4000)
    yield from c.checkpoint("career")
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from _cov_b_hole(c, (330, 230), (520, 330))
    yield from c.checkpoint("hole")
    yield from _cov_b_season(c, 40000)
    yield from c.checkpoint("play")
    c.write32(COV_B_DATE, 0x0FC0, tag="date -> before tournament offer"); yield from c.sleep(500)
    yield from _cov_b_season(c, 20000, esc_every=3000)
    yield from c.checkpoint("offer")
    c.write32(COV_B_DATE, 0x1FC0, tag="date -> before year end"); yield from c.sleep(500)
    yield from _cov_b_season(c, 30000, esc_every=3000)
    yield from c.checkpoint("year_end")
    yield from _cov_b_season(c, 60000)
    yield from c.checkpoint("year2")


def cov_b_locations(c):
    """Sandbox in the desert (Phoenix): rotations, zooms, a hole, edit mode, play."""
    yield from _cov_b_menu(c)
    yield from _cov_b_sandbox(c, "phoenix")
    yield from c.checkpoint("phoenix")
    yield from _cov_b_views(c)
    yield from _cov_b_hole(c, (330, 230), (520, 330))
    yield from _cov_b_edit(c)
    yield from _cov_b_season(c, 30000)
    yield from c.checkpoint("end")


def cov_b_title(c):
    """Title-menu branches: boot with Themes/Default.pro present (Setup 0x0040f8d6 loadThemeFile), Select A Theme ->
    Firaxis -> check, Sandbox -> location back arrow (startNewCourse 0 -> Session 0x0040fb2e), Start New Game -> difficulty
    back arrow (-1, menu head), then Start New Game -> Impossible -> Northeast: a career at difficulty 3 on the Firaxis
    theme, a hole, play."""
    yield from _cov_b_menu(c)
    yield from c.checkpoint("menu")
    yield from c.click(630, 425); yield from c.sleep(5000)      # Select A Theme
    yield from c.click(100, 109); yield from c.sleep(1500)      # Firaxis row
    yield from c.click(613, 552); yield from c.sleep(5000)      # check
    yield from c.checkpoint("theme")
    yield from c.click(130, 445); yield from c.sleep(5000)      # Sandbox Mode
    yield from c.click(768, 557); yield from c.sleep(5000)      # location back arrow
    yield from c.click(560, 110); yield from c.sleep(4000)      # Start New Game
    yield from c.click(765, 557); yield from c.sleep(4000)      # difficulty back arrow
    yield from c.click(400, 540); yield from c.sleep(3000)      # Play a Championship
    yield from c.click(390, 125); yield from c.sleep(4000)      # Easy
    yield from c.click(768, 557); yield from c.sleep(4000)      # course list back arrow (startNewCourse 0: 0x0040fb2e)
    yield from c.checkpoint("menu2")
    yield from c.click(560, 110); yield from c.sleep(4000)      # Start New Game
    yield from c.click(400, 476); yield from c.sleep(8000)      # Impossible
    yield from c.click(*COV_B_LOC["northeast"])
    yield from _cov_b_hud(c)
    yield from c.sleep(4000)
    yield from c.checkpoint("career")
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from _cov_b_hole(c, (330, 230), (480, 330))
    yield from c.click(118, 495); yield from c.sleep(1500)      # a building with no path to it (monthly path warning)
    yield from _cov_b_tool(c, COV_B_BLDG[1], [(560, 260), (580, 270)])
    yield from c.click(118, 495); yield from c.sleep(1200)
    yield from _cov_b_season(c, 20000)
    d = _cov_b_obs(c, "date", lambda: _cov_b_read_u32(c, COV_B_DATE))
    c.write32(COV_B_DATE, (d | 0x3FF) - 0x30, tag="date -> 48 before the next month"); yield from c.sleep(500)
    yield from _cov_b_season(c, 30000)
    yield from c.checkpoint("month")
    yield from _cov_b_season(c, 30000)
    yield from c.checkpoint("end")


def _cov_b_drag(c, pts, hold_ms=400):
    """Left-button drag through `pts` (WM_LBUTTONDOWN at the first, WM_MOUSEMOVE with MK_LBUTTON at each, up at the last)."""
    x0, y0 = pts[0]
    c.move(x0, y0); yield from c.sleep(400)
    c.act(0, k="vkey", vk=1, st=2, tag=f"drag down {x0},{y0}")
    c.act(0, k="post", msg=0x201, wp=1, lp=c.lp(x0, y0))
    yield from c.sleep(hold_ms)
    for x, y in pts[1:]:
        c.act(0, k="cursor", x=int(x), y=int(y))
        c.act(0, k="post", msg=0x200, wp=1, lp=c.lp(x, y))
        yield from c.sleep(hold_ms)
    x1, y1 = pts[-1]
    c.act(0, k="vkey", vk=1, st=1)
    c.act(0, k="post", msg=0x202, wp=0, lp=c.lp(x1, y1))
    yield from c.sleep(600)


# golf palette page 2 (wrench tab (235, 527)): popup buttons and the candidate item points above them
COV_B_PAGE2_POPUPS = [((468, 575), [(470, 495), (505, 495), (470, 522), (505, 522)]),
                ((530, 575), [(480, 505), (530, 505), (580, 505), (630, 505)]),
                ((592, 575), [(440, 495), (480, 495), (520, 495), (560, 495), (600, 495), (640, 495), (680, 495),
                              (720, 495)]),
                ((655, 575), [(430, 495), (480, 495), (530, 495), (580, 495), (630, 495), (680, 495), (730, 495),
                              (455, 522), (505, 522), (555, 522)]),
                ((715, 575), [(610, 495), (650, 495), (690, 495), (730, 495), (630, 522), (670, 522), (710, 522)])]
COV_B_PAGE2_ITEMS = [(327, 522), (305, 557), (365, 557)]
COV_B_TERRAIN_TOOLS = [(350, 545), (465, 545), (585, 545), (715, 555)]


def cov_b_decor(c):
    """Sandbox Monterey: golf palette page 2 (cart path drags, its two left items, every popup item of the five round
    buttons: benches, bridges, landmarks, flower beds, scenic trees) placed on the map; the terrain page of the
    buildings palette (three brushes and its fourth tool) dragged over the map; every rotation once more; play."""
    yield from _cov_b_menu(c)
    yield from _cov_b_sandbox(c, "monterey")
    yield from c.checkpoint("sandbox")
    yield from _cov_b_hole(c, (300, 200), (520, 330))
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from c.click(235, 527); yield from c.sleep(1500)
    spots = [(380, 260), (450, 300), (560, 240), (330, 330), (620, 330), (420, 380), (500, 200), (260, 280)]
    k = 0
    for item in COV_B_PAGE2_ITEMS:
        yield from c.click(*item); yield from c.sleep(800)
        x, y = spots[k % len(spots)]; k += 1
        yield from _cov_b_drag(c, [(x, y), (x + 30, y + 15), (x + 60, y + 30), (x + 90, y + 45), (x + 60, y + 60)])
        c.move(x + 20, y - 20); yield from c.sleep(800)
        yield from c.click(x - 40, y, right=True); yield from c.sleep(800)
    yield from c.checkpoint("paths")
    for btn, items in COV_B_PAGE2_POPUPS:
        for it in items:
            yield from c.click(*btn); yield from c.sleep(1000)
            yield from c.click(*it); yield from c.sleep(800)
            x, y = spots[k % len(spots)]; k += 1
            c.move(x, y); yield from c.sleep(700)
            c.move(x + 25, y + 10); yield from c.sleep(700)
            yield from c.click(x, y); yield from c.sleep(1200)
            yield from c.click(x - 40, y + 20, right=True); yield from c.sleep(800)
    yield from c.checkpoint("decor")
    yield from c.key(0x1B); yield from c.sleep(1000)
    yield from c.click(118, 495); yield from c.sleep(1500)
    yield from c.click(235, 527); yield from c.sleep(1500)
    for tool in COV_B_TERRAIN_TOOLS:
        yield from c.click(*tool); yield from c.sleep(1000)
        x, y = spots[k % len(spots)]; k += 1
        c.move(x, y); yield from c.sleep(800)
        yield from _cov_b_drag(c, [(x, y), (x + 20, y + 10), (x + 40, y + 20)], hold_ms=600)
        yield from c.click(x, y + 40); yield from c.sleep(1000)
        yield from c.click(x - 40, y + 20, right=True); yield from c.sleep(800)
    yield from c.checkpoint("terrain")
    yield from c.key(0x1B); yield from c.sleep(1000)
    yield from _cov_b_views(c)
    yield from _cov_b_season(c, 40000)
    yield from c.checkpoint("end")


def _cov_b_exit_to_menu(c):
    """wrench -> Exit the Game -> 'So long for now.' -> title menu."""
    yield from c.key(0x1B); yield from c.sleep(1000)
    yield from c.click(132, 580); yield from c.sleep(2000)
    yield from c.click(200, 535); yield from c.sleep(3000)
    yield from c.click(330, 212); yield from c.sleep(2000)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "main_menu.png"), name="main_menu_again")
    yield from c.sleep(2000)


def cov_b_career_types(c):
    """Careers (Start New Game -> Easy) at locations of every course type, chained through wrench -> Exit the Game ->
    'So long for now.' (the title menu again). The cash dword g_cash_div100 0x00571fd4 is set to 0x2710 ($1,000,000)
    while the location screen is up so every location can be bought (injection); each course is generated by
    generateRandomCourse 0x00470a60 (Session normal path 0x0040fbbd), then viewed in every rotation and zoom. No tropical
    location: a career at Hawaii or Oahu kills the stock game (hooks off) right after the purchase, access violation
    reading 0x8 at 0x00473e67 (Image_draw) called from 0x0043d990 (log/c3/alt/c3fyb/xb_hawaii/oahu/summary.json)."""
    yield from _cov_b_menu(c)
    for i, loc in enumerate(("scotland", "ireland", "lasvegas", "carolina", "novascotia", "wales")):
        yield from c.click(560, 110); yield from c.sleep(3000)      # Start New Game
        yield from c.click(390, 125); yield from c.sleep(6000)      # Easy
        c.write32(COV_B_CASH, 0x2710, tag="cash -> $1,000,000 on the location screen"); yield from c.sleep(1500)
        yield from c.click(*COV_B_LOC[loc])
        yield from _cov_b_hud(c, name=f"hud_{loc}")
        yield from c.sleep(3000)
        yield from c.key(0x1B); yield from c.sleep(1500)
        yield from c.checkpoint(f"career_{loc}")
        yield from _cov_b_views(c)
        yield from _cov_b_season(c, 12000)
        yield from c.checkpoint(f"played_{loc}")
        yield from _cov_b_exit_to_menu(c)


def _cov_b_obs(c, name, fn):
    """A decision from game memory, taken in the RECORD run and kept in the schedule ("obs", keys "cb<i>:<name>");
    replay arms read it back, so every arm runs the same input."""
    i = getattr(c, "cb_obs_index", 0)
    c.cb_obs_index = i + 1
    key = f"cb{i}:{name}"
    store = c.run.schedule.setdefault("obs", {})
    if c.run.mode == "replay":
        return store[key]
    v = fn()
    store[key] = v
    return v


def _cov_b_read_s16(c, va):
    return int(c.run.g.scripts[-1].exports_sync.s36a_read_s16(va))


COV_B_HOLES_PLUS1 = 0x005685F0
# candidate (tee, green) pairs at zoom 1 (whole map in view): long, medium, short and adjacent ones
COV_B_HOLE_PAIRS = [((150, 150), (450, 330)), ((500, 120), (700, 260)), ((250, 400), (330, 440)), ((600, 380), (640, 400)),
              ((100, 300), (300, 200)), ((420, 160), (560, 180)), ((700, 140), (720, 150)), ((350, 300), (520, 420)),
              ((200, 250), (230, 262)), ((560, 300), (620, 340)), ((300, 120), (380, 100)), ((650, 430), (720, 400)),
              ((120, 420), (180, 380)), ((470, 250), (540, 270)), ((260, 180), (330, 210)), ((400, 380), (430, 360)),
              ((150, 220), (160, 240)), ((680, 320), (740, 300)), ((520, 440), (580, 460)), ((330, 250), (360, 270)),
              ((220, 330), (250, 350)), ((600, 200), (640, 220)), ((450, 420), (480, 440)), ((380, 200), (400, 215)),
              ((100, 380), (130, 400)), ((720, 220), (760, 240)), ((280, 450), (310, 470)), ((560, 120), (590, 140)),
              ((180, 120), (210, 140)), ((480, 330), (500, 345))]


def cov_b_holes18(c):
    """Sandbox Monterey: five holes whose green is placed after moving the camera 0, 2, 4, 6 and 8 arrow presses (4
    tiles each) away from the tee, so the build HUD shows every length class (0x00417395..0x004173c4: <= 50, < 250,
    < 475, <= 625, > 625 yards) before each opens; then holes from COV_B_HOLE_PAIRS until 18 are open (or the list ends);
    Shift held over the course (P6 0x00418d72: > 10 holes); a tee and green past 18 (the 18-hole limit, H1 0x0041ec50
    and P4 0x00415bf0); then play."""
    yield from _cov_b_menu(c)
    yield from _cov_b_sandbox(c, "monterey")
    yield from c.key(0x58); yield from c.sleep(1200)
    yield from c.key(0x58); yield from c.sleep(1200)
    yield from c.checkpoint("zoom1")
    yield from c.key(0x5A); yield from c.sleep(1000)
    yield from c.key(0x5A); yield from c.sleep(1000)
    yield from c.click(45, 470); yield from c.sleep(1200)
    for k, (fwd, back) in zip((0, 2, 4, 6, 8), ((0x27, 0x25), (0x28, 0x26), (0x25, 0x27), (0x26, 0x28), (0x27, 0x25))):
        yield from c.click(300, 527); yield from c.sleep(700)
        yield from c.click(400, 280); yield from c.sleep(1000)
        for _ in range(k):
            yield from c.key(fwd); yield from c.sleep(700)
        yield from c.click(365, 527); yield from c.sleep(700)
        c.move(380, 290); yield from c.sleep(600)
        yield from c.click(400 + (40 if k == 0 else 0), 280 + (20 if k == 0 else 0)); yield from c.sleep(1500)
        yield from c.click(300, 350, right=True); yield from c.sleep(2500)  # the hole HUD shows before it opens
        yield from c.key(0x48); yield from c.sleep(2000)
        yield from c.key(0x1B); yield from c.sleep(800)
        for _ in range(k):
            yield from c.key(back); yield from c.sleep(700)
    yield from c.checkpoint("long_holes")
    for i, (tee, green) in enumerate(COV_B_HOLE_PAIRS):
        n = _cov_b_obs(c, f"holes{i}", lambda: _cov_b_read_s16(c, COV_B_HOLES_PLUS1) - 1)
        if n >= 18:
            break
        yield from c.click(300, 527); yield from c.sleep(700)
        yield from c.click(*tee); yield from c.sleep(1000)
        yield from c.click(365, 527); yield from c.sleep(700)
        c.move(green[0] - 20, green[1]); yield from c.sleep(600)
        yield from c.click(*green); yield from c.sleep(1500)
        yield from c.click(300, 350, right=True); yield from c.sleep(1200)
        yield from c.key(0x48); yield from c.sleep(2000)
        yield from c.key(0x1B); yield from c.sleep(800)
    yield from c.checkpoint("holes")
    # employees (hireEmployee 0x0040aa80 needs 6 holes: string 0x004c5228): the people palette's eight slots, each
    # opening the hire dialog (showHireEmployeeDialog 0x00459400) where the first candidate is taken
    yield from c.click(45, 470); yield from c.sleep(1000)
    yield from c.click(175, 535); yield from c.sleep(2500)
    for slot in ((368, 530), (432, 530), (496, 530), (560, 530), (368, 575), (432, 575), (496, 575), (560, 575)):
        c.move(*slot); yield from c.sleep(800)
        yield from c.click(*slot); yield from c.sleep(3000)
        yield from c.click(330, 318); yield from c.sleep(3000)
        yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.click(175, 535); yield from c.sleep(1500)
    yield from c.click(45, 470); yield from c.sleep(1200)
    yield from c.checkpoint("hired")
    c.act(0, k="vkey", vk=0x10, st=2, tag="shift down"); yield from c.sleep(3000)
    c.act(0, k="vkey", vk=0x10, st=1, tag="shift up"); yield from c.sleep(500)
    for tee, green in (((330, 250), (420, 300)), ((200, 200), (260, 230))):
        yield from c.click(300, 527); yield from c.sleep(700)
        yield from c.click(*tee); yield from c.sleep(1200)
        yield from c.key(0x1B); yield from c.sleep(1500)
        yield from c.click(365, 527); yield from c.sleep(700)
        yield from c.click(*green); yield from c.sleep(1500)
        yield from c.click(300, 350, right=True); yield from c.sleep(1000)
        yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.checkpoint("limit")
    yield from c.click(45, 470); yield from c.sleep(1000)
    yield from _cov_b_season(c, 40000)
    yield from c.checkpoint("end")


def cov_b_shots(c, cap_s=900, max_shots=40):
    """Championship at Moderate: on Gary's turn every shot-type key ('h' 3, 'l' 4, 's' 0, 'f' -1, 'd' 1: [0x0058f330],
    H1 0x0041c5b9..0x0041c6f3) with the aim hovered near, mid and far (P7 drag preview 0x0041b20f..0x0041c0c5: rateTile
    modes, club rows, swing arc), then a straight shot at the pin (scenario.py's finder, decided in the record run)."""
    import scenario as SCN
    shot = lambda: c.run.g.shot()
    yield from _cov_b_menu(c)
    yield from c.click(400, 540); yield from c.sleep(2500)    # Play a Championship
    yield from c.click(390, 238); yield from c.sleep(3000)    # Moderate
    yield from c.click(380, 122); yield from c.sleep(1000)    # first course in the list
    yield from c.click(630, 555); yield from c.sleep(4000)    # OK
    yield from c.click(360, 122); yield from c.sleep(1000)    # Gary Golf.pro
    yield from c.click(632, 553); yield from c.sleep(8000)    # OK -> tournament
    yield from c.checkpoint("tournament_start")
    shots, t0 = 0, c.T
    while c.T - t0 < cap_s * 1000 and shots < max_shots:
        st = _cov_b_obs(c, "tstate", lambda: "over" if SCN.tournament_over(shot()) else
                        ("gary" if imgcmp.matches(shot().crop(SCN.SHOT_PANEL), imgcmp.load(GOLDEN / "shot_panel.png"), 25)
                         else "other"))
        if st == "over":
            yield from c.sleep(2000)
            yield from c.checkpoint("results")
            yield from c.click(*SCN.RESULTS_OK); yield from c.sleep(3000)
            break
        if st == "gary":
            green = _cov_b_obs(c, "green", lambda: SCN.find_green(shot()))
            target = _cov_b_obs(c, "target", lambda: (SCN.find_pin(shot(), green) if green else None) or green or (400, 260))
            for ch in "hlsfd"[shots % 5:] + "hlsfd"[:shots % 5]:
                yield from c.key(ord(ch.upper()), char=ch); yield from c.sleep(500)
                for (dx, dy) in ((0, 0), (-120, 60), (150, -80)):
                    c.move(target[0] + dx, target[1] + dy); yield from c.sleep(500)
            yield from c.click(*SCN.SHOT_STRAIGHT); yield from c.sleep(800)
            for (dx, dy) in ((-200, 100), (60, 30)):
                c.move(target[0] + dx, target[1] + dy); yield from c.sleep(500)
            c.move(*target); yield from c.sleep(600)
            yield from c.click(*target)
            shots += 1
            yield from c.sleep(2000)
            if shots % 8 == 0:
                yield from c.checkpoint(f"shot_{shots:02d}")
            continue
        yield from c.sleep(700)
        yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.sleep(3000)
    yield from c.checkpoint("end")


def _cov_b_read_u32(c, va):
    return int(c.run.g.scripts[-1].exports_sync.s36a_read_s32(va)) & 0xFFFFFFFF


def _cov_b_set_bits(c, va, bits, name, clear=False):
    """Read-modify-write of a dword on the main thread: the value is read in the RECORD run (kept in the schedule) and
    the same result is written in every arm (injection)."""
    v = _cov_b_obs(c, name, lambda: _cov_b_read_u32(c, va))
    c.write32(va, (v & ~bits) if clear else (v | bits), tag=f"{name} {'&~' if clear else '|'}{bits:#x}")


def cov_b_inject(c, holes=1):
    """States no input of the stock game makes, set by injection on a sandbox course with a hole, a building and a home
    site, so the transcription of the blocks behind them is A/B compared (log/c3/c3fyb_notes.txt lists each with its
    writer scan): bit 0x20 of g_flags 0x0059e7b8 (debug tile overlays; no instruction sets it), the zoom-box word
    0x005a9cbc (only ever stored as 0, 0x0041c8ef; 'x' clears it), bits 0x40 / 0x80 of 0x0056c7b4 (only set from
    0x00543cf8, which is only stored as 0), the word 0x0059b04c (no instruction stores it)."""
    yield from _cov_b_menu(c)
    yield from _cov_b_sandbox(c, "monterey")
    yield from _cov_b_hole(c, (300, 200), (520, 330))
    if holes == 2:
        # a second hole: the magazine issue block needs byte +0x200 of hole record 2 (0x005760c8 + 0x208 *
        # [0x0059b730], 0x0040ff59) set
        yield from c.click(45, 470); yield from c.sleep(1000)
        yield from c.click(300, 527); yield from c.sleep(700)
        yield from c.click(620, 260); yield from c.sleep(1000)
        yield from c.click(365, 527); yield from c.sleep(700)
        yield from c.click(450, 420); yield from c.sleep(1500)
        yield from c.click(300, 350, right=True); yield from c.sleep(1000)
        yield from c.key(0x48); yield from c.sleep(2000)
        yield from c.key(0x1B); yield from c.sleep(1000)
        yield from c.click(45, 470); yield from c.sleep(1000)
    yield from c.click(118, 495); yield from c.sleep(1500)
    for slot, spot in ((COV_B_BLDG[0], (420, 280)), (COV_B_BLDG[1], (600, 250)), (COV_B_BLDG[7], (250, 330))):
        yield from _cov_b_tool(c, slot, [spot, (spot[0] + 20, spot[1] + 10)])
        yield from c.click(118, 495); yield from c.sleep(800)
        yield from c.click(118, 495); yield from c.sleep(800)
    yield from c.click(118, 495); yield from c.sleep(1000)
    # membership level 3 for golfer types 0..5 (byte +2 of the 0x2c-byte records at 0x005849e0): holeMagazineEvent
    # 0x0042dea0 then counts home sites into 0x0056d1b0, which lets the golf palette page-2 home-site item pick tool 5
    # (clickAmenityPanel 0x00434b04)
    for t in range(6):
        va = 0x005849E0 + 0x2C * t
        v = _cov_b_obs(c, f"type{t}", lambda va=va: _cov_b_read_u32(c, va))
        c.write32(va, (v & 0xFFF8FFFF) | 0x00030000, tag=f"type {t} level 3")
    yield from c.sleep(2000)
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from c.click(235, 527); yield from c.sleep(1500)
    for item, spot in zip(COV_B_PAGE2_ITEMS, ((450, 260), (560, 300), (380, 340))):
        yield from c.click(*item); yield from c.sleep(800)
        c.move(*spot); yield from c.sleep(1200)
        c.move(spot[0] + 30, spot[1] + 10); yield from c.sleep(1200)
        yield from c.click(*spot); yield from c.sleep(1500)
        yield from c.click(spot[0] - 40, spot[1], right=True); yield from c.sleep(800)
    yield from c.click(45, 470); yield from c.sleep(1000)
    yield from c.checkpoint("course")
    # debug overlays: plain, then with terrain edit mode (P1 grid numbers need bit 2 too)
    _cov_b_set_bits(c, COV_B_FLAGS, 0x20, "flags20_on"); yield from c.sleep(4000)
    yield from c.key(0x45); yield from c.sleep(3000)
    c.move(400, 300); yield from c.sleep(1500)
    yield from c.key(0x45); yield from c.sleep(1500)
    yield from c.key(0x45); yield from c.sleep(1000)
    yield from c.key(0x1B); yield from c.sleep(1000)
    _cov_b_set_bits(c, COV_B_FLAGS, 0x20, "flags20_off", clear=True); yield from c.sleep(1000)
    yield from c.checkpoint("debug_overlay")
    c.write32(0x005A9CBC, 1, tag="zoom box on"); yield from c.sleep(4000)
    yield from c.key(0x58); yield from c.sleep(1500)                   # 'x' clears it (0x0041c8ef)
    yield from c.checkpoint("zoom_box")
    c.write32(0x0059B04C, 1, tag="0x59b04c on"); yield from c.sleep(3000)
    c.write32(0x0059B04C, 0, tag="0x59b04c off"); yield from c.sleep(1000)
    _cov_b_set_bits(c, 0x0056C7B4, 0xC0, "mask56c7b4")
    yield from _cov_b_season(c, 30000)
    d = _cov_b_obs(c, "date", lambda: _cov_b_read_u32(c, COV_B_DATE))
    c.write32(COV_B_DATE, (d | 0x3FF) - 0x30, tag="date -> 48 before the next month"); yield from c.sleep(500)
    yield from c.checkpoint("forced_events")
    yield from _cov_b_season(c, 30000)
    yield from c.checkpoint("end")


COV_B_PREF_BOXES = [(236, 245), (236, 269), (236, 293), (236, 317), (236, 341), (236, 365)]
COV_B_PREF_OK = (560, 368)


def _cov_b_prefs(c, boxes):
    yield from c.click(132, 580); yield from c.sleep(2000)
    yield from c.click(200, 487); yield from c.sleep(3000)     # Edit Preferences
    for b in boxes:
        yield from c.click(*b); yield from c.sleep(700)
    yield from c.click(*COV_B_PREF_OK); yield from c.sleep(2000)


def cov_b_people(c):
    """Sandbox Monterey with two holes: every preference box off (the [0x005a5a00] bits P2 0x00412615 / P4 0x00416d43
    read), play, every box back on; the '+' key (cash + 100, bit 0x20000000 of g_flags: P6 0x0041955b); play."""
    yield from _cov_b_menu(c)
    yield from _cov_b_sandbox(c, "monterey")
    yield from _cov_b_hole(c, (300, 200), (520, 330))
    yield from c.click(45, 470); yield from c.sleep(1000)
    yield from c.click(300, 527); yield from c.sleep(700)
    yield from c.click(620, 260); yield from c.sleep(1000)
    yield from c.click(365, 527); yield from c.sleep(700)
    yield from c.click(450, 420); yield from c.sleep(1500)
    yield from c.click(300, 350, right=True); yield from c.sleep(1000)
    yield from c.key(0x48); yield from c.sleep(2000)
    yield from c.key(0x1B); yield from c.sleep(1000)
    yield from c.click(45, 470); yield from c.sleep(1000)
    yield from c.checkpoint("course")
    yield from _cov_b_prefs(c, COV_B_PREF_BOXES)
    yield from _cov_b_season(c, 20000)
    yield from c.checkpoint("prefs_off")
    yield from _cov_b_prefs(c, COV_B_PREF_BOXES)
    yield from c.checkpoint("prefs_on")
    yield from c.key(0xBB, char="+"); yield from c.sleep(2000)
    yield from _cov_b_season(c, 30000)
    # idle hover over the golfers (P7 0x0041aa36: hovered id [0x004c2848] >= 0x80), positions from the record frame
    import scenario as SCN
    for r in range(3):
        labels = _cov_b_obs(c, f"labels{r}", lambda: [list(p) for p in SCN.find_golfer_labels(c.run.g.shot())[:5]])
        for x, y in labels:
            for dy in (8, 14, 22):
                c.move(x, y + dy); yield from c.sleep(700)
        yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.checkpoint("hover")
    yield from _cov_b_season(c, 30000)
    yield from c.checkpoint("end")


def _cov_b_clicks(c, pts, settle=500):
    for x, y in pts:
        yield from c.click(x, y, settle=200); yield from c.sleep(settle)


def cov_b_paths(c):
    """Sandbox Monterey: pathway tiles (golf palette page 2, first item: kind 0, tile flag 0x20 set at 0x004209a5) clicked
    one by one into a straight run, a branch, a dead end and a run that ends at the river; raised terrain with water
    painted across the slope (water steps: P1 0x00410ff8 / 0x00411282); every rotation; play."""
    yield from _cov_b_menu(c)
    yield from _cov_b_sandbox(c, "monterey")
    yield from _cov_b_hole(c, (300, 200), (520, 330))
    yield from c.click(45, 470); yield from c.sleep(1200)
    yield from c.click(235, 527); yield from c.sleep(1200)
    yield from c.click(*COV_B_PAGE2_ITEMS[0]); yield from c.sleep(800)
    run = [(330 + 16 * k, 250 + 8 * k) for k in range(10)]
    branch = [(410 - 16 * k, 290 + 8 * k) for k in range(1, 6)]
    to_river = [(250 + 16 * k, 330 - 8 * k) for k in range(8)] + [(200 - 16 * k, 330 + 8 * k) for k in range(6)]
    yield from _cov_b_clicks(c, run + branch + to_river)
    yield from c.click(500, 400, right=True); yield from c.sleep(1000)
    yield from c.checkpoint("paths")
    # raise a block of terrain: terrain page of the buildings palette, first brush, clicks on one spot
    yield from c.click(235, 527); yield from c.sleep(1000)
    yield from c.click(45, 470); yield from c.sleep(1000)
    yield from c.click(118, 495); yield from c.sleep(1200)
    yield from c.click(235, 527); yield from c.sleep(1200)
    yield from c.click(*COV_B_TERRAIN_TOOLS[0]); yield from c.sleep(800)
    yield from _cov_b_clicks(c, [(560, 220)] * 6 + [(590, 235)] * 6 + [(530, 205)] * 4, settle=400)
    yield from c.click(500, 400, right=True); yield from c.sleep(800)
    yield from c.click(118, 495); yield from c.sleep(1000)
    yield from c.checkpoint("raised")
    yield from c.click(45, 470); yield from c.sleep(1200)
    for slot in (COV_B_GOLF_TOP[5], COV_B_GOLF_TOP[6]):
        yield from c.click(*slot); yield from c.sleep(800)
        yield from _cov_b_drag(c, [(500, 190), (530, 205), (560, 220), (590, 235), (620, 250), (650, 265)])
        yield from _cov_b_clicks(c, [(545, 213), (575, 228), (605, 243)])
        yield from c.click(500, 400, right=True); yield from c.sleep(800)
    yield from c.click(45, 470); yield from c.sleep(1000)
    yield from c.checkpoint("water")
    yield from _cov_b_views(c)
    yield from _cov_b_season(c, 30000)
    yield from c.checkpoint("end")


def cov_b_shots2(c, cap_s=900, max_shots=40):
    """Championship at Difficult: Gary aims short, wide or long of the pin by turns (lies in rough, sand and water feed
    the P7 club-name suffix switch 0x004217f4 and the club rows 0x00421818), each preceded by the shot-type keys."""
    import scenario as SCN
    shot = lambda: c.run.g.shot()
    yield from _cov_b_menu(c)
    yield from c.click(400, 540); yield from c.sleep(2500)    # Play a Championship
    yield from c.click(390, 356); yield from c.sleep(3000)    # Difficult
    yield from c.click(380, 122); yield from c.sleep(1000)
    yield from c.click(630, 555); yield from c.sleep(4000)
    yield from c.click(360, 122); yield from c.sleep(1000)
    yield from c.click(632, 553); yield from c.sleep(8000)
    yield from c.checkpoint("tournament_start")
    offsets = [(-90, 45), (110, -40), (0, 0), (60, 70), (-140, -30)]
    shots, t0 = 0, c.T
    while c.T - t0 < cap_s * 1000 and shots < max_shots:
        st = _cov_b_obs(c, "tstate", lambda: "over" if SCN.tournament_over(shot()) else
                        ("gary" if imgcmp.matches(shot().crop(SCN.SHOT_PANEL), imgcmp.load(GOLDEN / "shot_panel.png"), 25)
                         else "other"))
        if st == "over":
            yield from c.sleep(2000)
            yield from c.checkpoint("results")
            yield from c.click(*SCN.RESULTS_OK); yield from c.sleep(3000)
            break
        if st == "gary":
            green = _cov_b_obs(c, "green", lambda: SCN.find_green(shot()))
            target = _cov_b_obs(c, "target", lambda: (SCN.find_pin(shot(), green) if green else None) or green or (400, 260))
            dx, dy = offsets[shots % len(offsets)]
            aim = (min(max(target[0] + dx, 20), 780), min(max(target[1] + dy, 80), 460))
            for ch in "dfslh"[shots % 5:] + "dfslh"[:shots % 5]:
                yield from c.key(ord(ch.upper()), char=ch); yield from c.sleep(400)
                c.move(*aim); yield from c.sleep(400)
            yield from c.click(*SCN.SHOT_STRAIGHT); yield from c.sleep(800)
            c.move(*aim); yield from c.sleep(600)
            yield from c.click(*aim)
            shots += 1
            yield from c.sleep(2000)
            if shots % 8 == 0:
                yield from c.checkpoint(f"shot_{shots:02d}")
            continue
        yield from c.sleep(700)
        yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.sleep(3000)
    yield from c.checkpoint("end")


def cov_b_inject2(c):
    """cov_b_inject on a course with two holes (the magazine issue block 0x0040ff4d..0x00410394 needs hole record 2
    open, 0x0040ff59)."""
    yield from cov_b_inject(c, holes=2)


SCENARIOS["cov_b_build"] = {"env": {}, "phases": [], "lockstep": cov_b_build}
SCENARIOS["cov_b_career"] = {"env": {}, "phases": [], "lockstep": cov_b_career}
SCENARIOS["cov_b_locations"] = {"env": {}, "phases": [], "lockstep": cov_b_locations}
SCENARIOS["cov_b_title"] = {"env": {}, "phases": [], "lockstep": cov_b_title, "files": {"Themes/Default.pro": "log/c3/alt/s36a/pristine/Themes/Championship/Gary Golf.pro"}}
SCENARIOS["cov_b_decor"] = {"env": {}, "phases": [], "lockstep": cov_b_decor}
SCENARIOS["cov_b_career_types"] = {"env": {}, "phases": [], "lockstep": cov_b_career_types}
SCENARIOS["cov_b_holes18"] = {"env": {}, "phases": [], "lockstep": cov_b_holes18}
SCENARIOS["cov_b_shots"] = {"env": {}, "phases": [], "lockstep": cov_b_shots}
SCENARIOS["cov_b_inject"] = {"env": {}, "phases": [], "lockstep": cov_b_inject}
SCENARIOS["cov_b_people"] = {"env": {}, "phases": [], "lockstep": cov_b_people}
SCENARIOS["cov_b_paths"] = {"env": {}, "phases": [], "lockstep": cov_b_paths}
SCENARIOS["cov_b_shots2"] = {"env": {}, "phases": [], "lockstep": cov_b_shots2}
SCENARIOS["cov_b_inject2"] = {"env": {}, "phases": [], "lockstep": cov_b_inject2}


# ---- c3fya: mainLoop H1 / H2 / quit coverage (batch c3fya, 2026-10-16; notes log/c3/c3fya_notes.txt) -------------------
# Lockstep (frame-locked Ctx) scripts for re/frida/scenario_ab.py, run through log/c3/c3fya_run.py, which boots them with
# log/c3/s36a_run.py's per-boot restore of saves / Themes / scores and its agent extension (the s36a_* rpc used below for
# record-run decisions: golfer-panel calls, the game date); cov_a_flags also needs the fixture saves that runner copies in.
# Every decision taken from the screen or memory is made in the RECORD run and replayed from the schedule ("obs").
# Characters for mainLoop's key switch 0x00421838 are posted as WM_CHAR alone (see _cova_char).

def _cova_to_sandbox(c):
    yield from c.wait_screen(imgcmp.load(GOLDEN / "main_menu.png"), name="main_menu")
    yield from c.sleep(3000)
    yield from c.checkpoint("menu")
    yield from c.click(130, 445)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "location.png"), name="location")
    yield from c.click(270, 32)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "ingame_hud_dial.png"), crop=DIAL, threshold=30, name="hud")
    yield from c.sleep(2000)
    yield from c.key(0x1B); yield from c.sleep(1500)


COVA_WM_KEYDOWN, COVA_WM_KEYUP, COVA_WM_CHAR = 0x100, 0x101, 0x102


def _cova_char(c, ch, settle=1500):
    """One character for mainLoop's H1 key switch ([F+0x10] = [0x822b9c], written by rootWin_onKey 0x00465750 from
    COVA_WM_CHAR). Posted as COVA_WM_CHAR alone: a posted COVA_WM_KEYDOWN would go through the game pump's TranslateMessage, which adds
    a COVA_WM_CHAR from the REAL keyboard state after ours (cov_a_probe2's watch log: vk 0x42 + 'B' left 'b' = 98 last)."""
    c.act(0, k="post", msg=COVA_WM_CHAR, wp=ord(ch), lp=1, tag=f"char {ch!r}")
    yield from c.sleep(settle)


def _cova_vk(c, vk, settle=1500):
    """A key-down / key-up pair for the report switch on [F+0x28] = [0x822b98] (rootWin_onChar 0x004657b0)."""
    yield from c.key(vk, char="")
    yield from c.sleep(settle)


def _cova_esc(c, n=1, settle=1500):
    for _ in range(n):
        yield from c.key(0x1B)
        yield from c.sleep(settle)


def _cova_hole(c, tee, green):
    yield from c.click(300, 527); yield from c.sleep(800)       # tee tool
    yield from c.click(*tee); yield from c.sleep(1200)
    yield from c.click(365, 527); yield from c.sleep(800)       # green tool
    yield from c.click(*green); yield from c.sleep(2000)
    yield from c.click(300, 350, right=True); yield from c.sleep(1000)
    yield from c.key(0x48); yield from c.sleep(3000)            # H: open the hole


def _cova_two_holes(c):
    yield from c.click(45, 470); yield from c.sleep(1500)       # Build Course palette
    yield from _cova_hole(c, (300, 200), (520, 330))
    yield from _cova_esc(c)
    yield from _cova_hole(c, (570, 380), (330, 280))
    yield from _cova_esc(c)
    yield from c.click(45, 470); yield from c.sleep(1500)       # close the palette


def cov_a_keys(c):
    """mainLoop H1: every character of the 57-case key switch 0x00421838 (index table 0x0042191c) and the report-key
    switch 0x004219b0 cases the old scenarios miss, in a sandbox with two open holes."""
    yield from _cova_to_sandbox(c)
    yield from _cova_two_holes(c)
    yield from c.checkpoint("holes")
    c.move(400, 300); yield from c.sleep(1000)
    for ch in "fgrstvw":                                         # 0x40d890 callers (mode 0)
        yield from _cova_char(c, ch)
        yield from _cova_esc(c)
    yield from c.checkpoint("k_speed")
    for ch in "dlhexz0":
        yield from _cova_char(c, ch)
    yield from _cova_esc(c)
    yield from c.checkpoint("k_misc")
    for ch in "pb":                                              # object tools 0 / 1, placed on the map, then Enter
        yield from _cova_char(c, ch)
        yield from c.click(250, 250); yield from c.sleep(2000)
        yield from c.click(450, 420); yield from c.sleep(2000)
        yield from _cova_char(c, "\r")
    yield from _cova_char(c, " ")
    yield from _cova_esc(c)
    yield from c.checkpoint("k_tools")
    for ch in "+NPT":                                            # P pauses (flag 4), T the terrain view (0x400)
        yield from _cova_char(c, ch)
        yield from _cova_esc(c)
    yield from _cova_char(c, "P")                                     # P, T again: both flags back
    yield from _cova_char(c, "T")
    yield from c.checkpoint("k_cheats")
    yield from _cova_char(c, "W", settle=3000)                        # W: the world map (0x00407d30); Esc does not close it
    yield from c.checkpoint("k_worldmap")
    yield from c.click(768, 557); yield from c.sleep(3000)       # its back arrow (ui_tour)
    for ch in "^?":
        yield from _cova_char(c, ch)
        yield from _cova_esc(c)
    yield from c.checkpoint("k_cheats2")
    for ch in "1239!@#$%&*(":                                    # hole select 1..18 (holes 1, 2 exist)
        yield from _cova_char(c, ch)
    yield from c.checkpoint("k_holes")
    c.move(420, 300); yield from c.sleep(1000)
    for ch in "===---":                                          # terrain raise / lower at the cursor tile
        yield from _cova_char(c, ch)
    yield from _cova_char(c, "\t")
    yield from c.checkpoint("k_terrain")
    for vk in (0x21, 0x22, 0x24, 0x23, 0x27, 0x28, 0x25, 0x26):  # report switch: PgUp, PgDn, Home, End, arrows
        yield from _cova_vk(c, vk)
    yield from c.checkpoint("k_vk")
    for ch in "SLRJ":                                            # dialogs: save, load browser, routing map, course report
        yield from _cova_char(c, ch, settle=3000)
        yield from c.checkpoint(f"k_dlg_{ch}")
        yield from _cova_esc(c, 2)
    yield from c.checkpoint("k_dialogs")
    yield from _cova_char(c, "C", settle=3000)                        # save course for championship (+ overwrite prompt)
    yield from c.checkpoint("k_C")
    yield from _cova_esc(c, 2)
    yield from _cova_char(c, "B", settle=3000)
    yield from c.checkpoint("k_B")
    yield from _cova_esc(c)
    yield from c.checkpoint("k_files")
    for ch in "./":                                              # routing render (12 holes, modal at hole 11)
        yield from _cova_char(c, ch, settle=4000)
        yield from c.checkpoint(f"k_render_{'dot' if ch == '.' else 'slash'}")
        yield from c.click(400, 300); yield from c.sleep(2000)
        yield from _cova_esc(c)
    yield from _cova_char(c, "n", settle=3000)
    yield from _cova_esc(c, 2)
    yield from c.checkpoint("k_render")
    for _ in range(10):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


# map spots (800x600 game space) on the open grass of the sandbox Monterey start view (cov_a_probe frames)
COVA_SPOTS = [(560, 230), (620, 290), (480, 260), (700, 330), (420, 300), (520, 360), (640, 400), (380, 250), (600, 180),
         (460, 330), (680, 250), (540, 300), (400, 350), (580, 420), (720, 380), (500, 200)]


COVA_TERRAIN_ROW1 = [(302, 527), (365, 527), (428, 527), (490, 527), (552, 527), (614, 527), (676, 527), (740, 527)]


COVA_TERRAIN_ROW2 = [(333, 570), (396, 570), (458, 570), (520, 570), (582, 570), (645, 570), (708, 570), (770, 570)]


COVA_BLDG_SLOTS = [(375, 520), (460, 515), (540, 510), (615, 520), (690, 525), (410, 565), (490, 560), (570, 555), (650, 560)]


def _cova_drag(c, a, b, steps=6, right=False, step_ms=200):
    """press at a, move to b in `steps`, release (virtual button state + posted messages, as Ctx.click does)."""
    vk = 0x02 if right else 0x01
    down, up, mk = (0x204, 0x205, 2) if right else (0x201, 0x202, 1)
    c.move(*a); yield from c.sleep(300)
    c.act(0, k="vkey", vk=vk, st=2, tag=f"drag down {a}")
    c.act(0, k="post", msg=down, wp=mk, lp=c.lp(*a))
    yield from c.sleep(step_ms)
    for i in range(1, steps + 1):
        x = a[0] + (b[0] - a[0]) * i // steps
        y = a[1] + (b[1] - a[1]) * i // steps
        c.act(0, k="cursor", x=x, y=y)
        c.act(0, k="post", msg=0x200, wp=mk, lp=c.lp(x, y))
        yield from c.sleep(step_ms)
    c.act(0, k="vkey", vk=vk, st=1)
    c.act(0, k="post", msg=up, wp=0, lp=c.lp(*b))
    yield from c.sleep(500)


def cov_a_build(c):
    """mainLoop H2: every terrain tool of the Build Course palette, the amenities, the buildings, the terrain-edit
    brushes and the undo right-click, each used on the map of a sandbox."""
    yield from _cova_to_sandbox(c)
    yield from c.click(45, 470); yield from c.sleep(1500)        # Build Course palette (terrain tab)
    k = 0
    for i, t in enumerate(COVA_TERRAIN_ROW1 + COVA_TERRAIN_ROW2):
        yield from c.click(*t); yield from c.sleep(800)
        p = COVA_SPOTS[k % len(COVA_SPOTS)]; k += 1
        yield from c.click(*p); yield from c.sleep(1200)
        yield from _cova_drag(c, p, (p[0] + 40, p[1] + 20))
        yield from c.sleep(800)
        yield from c.click(300, 350, right=True); yield from c.sleep(1000)
        if i % 4 == 3:
            yield from c.checkpoint(f"terrain_{i:02d}")
    yield from c.key(0x48); yield from c.sleep(3000)              # open a hole if a tee + green exist
    yield from _cova_esc(c)
    yield from c.click(235, 527); yield from c.sleep(1500)       # amenities tab
    yield from c.checkpoint("amenities")
    for sub in ((470, 575), (532, 575), (594, 575), (655, 575), (717, 575)):
        yield from c.click(*sub); yield from c.sleep(1200)
        for it in ((305, 555), (365, 560)):
            yield from c.click(*it); yield from c.sleep(800)
            p = COVA_SPOTS[k % len(COVA_SPOTS)]; k += 1
            yield from c.click(*p); yield from c.sleep(1500)
            yield from c.click(300, 350, right=True); yield from c.sleep(1000)
        yield from c.checkpoint(f"amen_{sub[0]}")
    yield from c.click(45, 470); yield from c.sleep(1500)        # close
    yield from c.click(118, 495); yield from c.sleep(1500)       # buildings
    for j, s in enumerate(COVA_BLDG_SLOTS):
        yield from c.click(*s); yield from c.sleep(1000)
        p = COVA_SPOTS[k % len(COVA_SPOTS)]; k += 1
        yield from c.click(*p); yield from c.sleep(2500)
        yield from c.click(300, 350, right=True); yield from c.sleep(1000)
        yield from _cova_esc(c)
        if j % 3 == 2:
            yield from c.checkpoint(f"bldg_{j}")
    yield from c.click(237, 527); yield from c.sleep(1500)       # terrain-edit brushes (grid view, flag 2)
    for b in ((350, 540), (465, 540), (585, 540), (720, 545)):
        yield from c.click(*b); yield from c.sleep(800)
        for p in (COVA_SPOTS[k % len(COVA_SPOTS)], COVA_SPOTS[(k + 1) % len(COVA_SPOTS)]):
            yield from c.click(*p); yield from c.sleep(1200)
            yield from c.click(*p, right=True); yield from c.sleep(1200)
        k += 2
        yield from c.checkpoint(f"brush_{b[0]}")
    yield from c.click(275, 567); yield from c.sleep(1500)       # undo help
    yield from _cova_esc(c)
    yield from c.click(118, 495); yield from c.sleep(1500)       # close buildings
    yield from c.click(500, 300, right=True); yield from c.sleep(1500)   # undo: show, then apply
    yield from c.click(500, 300, right=True); yield from c.sleep(2500)
    yield from c.checkpoint("undo")
    for _ in range(10):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


def _cova_exit_menu(c):
    yield from c.click(132, 580); yield from c.sleep(2000)       # wrench
    yield from c.click(200, 535); yield from c.sleep(3000)       # Exit the Game -> retirement prompt (3 answers)


def cov_a_quit(c):
    """mainLoop quit paths: the in-game Exit the Game prompt (wrench menu) with its three answers (stay, save first, leave),
    the title menu, then the title menu's exit icon: QUIT 0x0040fa29..0x0040fac8 (top10.sve, promptAndSaveFile, the
    closing Bink, the credits) and mainLoop's return; the run ends when the process exits."""
    yield from _cova_to_sandbox(c)
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from _cova_hole(c, (300, 200), (520, 330))
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from c.checkpoint("game")
    yield from _cova_exit_menu(c)
    yield from c.checkpoint("exit_prompt")
    yield from c.click(260, 160); yield from c.sleep(3000)       # "Wait, I don't want to quit yet!"
    yield from c.checkpoint("exit_stay")
    yield from _cova_exit_menu(c)
    yield from c.click(260, 211); yield from c.sleep(4000)       # "So long for now." -> quit save -> title
    yield from c.checkpoint("exit_leave")
    for _ in range(4):
        yield from c.sleep(1000)
    yield from c.checkpoint("title1")
    yield from c.click(130, 445)                                 # Sandbox Mode again (a second session)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "location.png"), name="location2")
    yield from c.click(270, 32)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "ingame_hud_dial.png"), crop=DIAL, threshold=30, name="hud2")
    yield from c.sleep(2000)
    yield from _cova_esc(c)
    yield from c.checkpoint("game2")
    yield from _cova_exit_menu(c)
    yield from c.click(260, 186); yield from c.sleep(3000)       # "I'd like to save this game first."
    yield from c.checkpoint("exit_save")
    yield from c.key(0x0D, char="\r"); yield from c.sleep(4000)  # accept the proposed save name
    yield from c.checkpoint("exit_saved")
    for _ in range(4):
        yield from c.sleep(1000)
    yield from c.checkpoint("title2")
    c.move(750, 553); yield from c.sleep(1500)
    c.run.exit_expected = True
    yield from c.click(750, 553); yield from c.sleep(3000)       # the title menu's exit icon
    for i in range(4):
        yield from c.checkpoint(f"quit_{i:02d}")
        yield from c.sleep(2000)
    yield from c.checkpoint("credits")
    yield from c.key(0x1B); yield from c.sleep(1500)             # the credits end on a key
    for _ in range(300):
        yield from c.sleep(1000)


cov_a_quit.expect_exit = True


def cov_a_quit_title(c):
    """mainLoop QUIT 0x0040fa29..0x0040fb04 from the first title menu: the exit icon (hotspot 5, selection -2 through the
    jump table 0x00421b04), top10.sve, promptAndSaveFile 0x00437910, playAudioFile 0x0043cce0 (SMSG_closingfinal.bik),
    showCreditsScreen 0x0044b9c0, the frame cleanup and `ret` at 0x0040fb04; the run ends when the process exits."""
    yield from c.wait_screen(imgcmp.load(GOLDEN / "main_menu.png"), name="main_menu")
    yield from c.sleep(3000)
    yield from c.checkpoint("menu")
    c.move(750, 553); yield from c.sleep(1500)
    yield from c.checkpoint("hover_exit")
    c.run.exit_expected = True
    yield from c.click(750, 553); yield from c.sleep(2500)
    for i in range(6):
        yield from c.checkpoint(f"quit_{i:02d}")
        yield from c.sleep(2000)
    yield from c.checkpoint("credits")
    yield from c.key(0x1B); yield from c.sleep(1500)             # the credits end on a key or a mouse button
    yield from c.checkpoint("after_esc")
    yield from c.click(400, 300); yield from c.sleep(1500)
    for _ in range(300):
        yield from c.sleep(1000)


cov_a_quit_title.expect_exit = True

def _cova_obs(c, name, fn):
    """A decision taken from the screen in the RECORD run, kept in the schedule ("obs") and read back by every replay
    (the method of log/c3/s36a_run.py obs())."""
    import json as _json
    i = getattr(c, "obs_index", 0)
    c.obs_index = i + 1
    key = f"{i}:{name}"
    store = c.run.schedule.setdefault("obs", {})
    if c.run.mode == "replay":
        return store[key]
    v = _json.loads(_json.dumps(fn()))
    store[key] = v
    return v


COVA_SHOT_PANEL = (330, 485, 800, 535)


def _cova_gary_turn(c):
    return _cova_obs(c, "gary", lambda: bool(imgcmp.matches(c.run.g.shot().crop(COVA_SHOT_PANEL),
                                                       imgcmp.load(GOLDEN / "shot_panel.png"), 25)))


def _cova_wait_gary(c, cap_ms=120_000):
    t0 = c.T
    while c.T - t0 < cap_ms:
        if (yield from _cova_gary_turn_g(c)):
            return True
        yield from c.sleep(700)
        yield from c.key(0x1B); yield from c.sleep(1300)
    return False


def _cova_gary_turn_g(c):
    return _cova_gary_turn(c)
    yield  # noqa: unreachable (makes this a generator for `yield from`)


def cov_a_champ(c):
    """mainLoop H1 keys in a championship (flag 0x1000000, Gary Golf on the course): the shot-type keys d / f / s / l / h
    in mode 3 ([0x567afc] == 3, Gary's turn), C with the tournament flag, X (the table reset, 0x0041ca20), and n (the
    quit-tournament prompt, 0x0041c84a), answered Cancel then Yes."""
    yield from c.wait_screen(imgcmp.load(GOLDEN / "main_menu.png"), name="main_menu")
    yield from c.sleep(3000)
    yield from c.checkpoint("menu")
    yield from c.click(400, 540); yield from c.sleep(2500)    # Play a Championship
    yield from c.click(390, 125); yield from c.sleep(3000)    # Easy
    yield from c.click(380, 122); yield from c.sleep(1000)    # first course in the list
    yield from c.click(630, 555); yield from c.sleep(4000)    # OK
    yield from c.click(360, 122); yield from c.sleep(1000)    # Gary Golf.pro
    yield from c.click(632, 553); yield from c.sleep(8000)    # OK -> tournament
    yield from c.checkpoint("tournament_start")
    ok = yield from _cova_wait_gary(c)
    _cova_obs(c, "gary_found", lambda: ok)
    yield from c.checkpoint("gary_turn")
    for ch in "fslhd":
        yield from _cova_char(c, ch, settle=1200)
    yield from c.checkpoint("shot_keys")
    yield from _cova_char(c, "C", settle=3000)
    yield from _cova_esc(c)
    yield from _cova_char(c, "J", settle=3000)
    yield from _cova_esc(c, 2)
    yield from c.checkpoint("champ_CJ")
    yield from _cova_char(c, ".", settle=4000)                      # routing render in a championship ([0x822c88] != 0)
    yield from c.click(400, 300); yield from c.sleep(2000)
    yield from _cova_esc(c)
    yield from c.checkpoint("champ_render")
    yield from _cova_char(c, "n", settle=3000)                      # quit-tournament prompt
    yield from c.checkpoint("n_prompt")
    yield from _cova_esc(c, 2)
    yield from c.checkpoint("n_cancel")
    yield from _cova_char(c, "X", settle=3000)                      # table reset (flag 0x1000000)
    yield from c.checkpoint("X")
    yield from _cova_char(c, "n", settle=3000)
    yield from c.click(275, 202); yield from c.sleep(4000)     # "Yes, cancel." (answer 0: 0x0041c8b9)
    yield from c.checkpoint("n_yes")
    for _ in range(8):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


COVA_GOLFER_PANEL_FN = 0x0045C560


COVA_PANEL_BUTTONS = [(312, 256), (376, 256), (440, 256), (504, 256)]


def _cova_agent(c):
    return c.run.g.scripts[-1].exports_sync


def _cova_season(c, seconds):
    t = 0
    while t < seconds * 1000:
        yield from c.key(0x1B)
        yield from c.sleep(1800)
        t += 2000


def _cova_open_golfer(c, tries=6):
    """Click under golfer name labels until the golfer info panel 0x0045c560 is entered again (RECORD decides from the
    s36a agent counter, the replay reads the decision back). Returns True when a panel opened."""
    for _try in range(tries):
        n0 = _cova_obs(c, "pcount", lambda: _cova_agent(c).s36a_call_count(COVA_GOLFER_PANEL_FN))
        labels = _cova_obs(c, "labels", lambda: find_golfer_labels(c.run.g.shot())[:6])
        for x, y in labels:
            for dy in (14, 8, 22):
                yield from c.click(x, y + dy)
                yield from c.sleep(1000)
                if _cova_obs(c, "pnow", lambda: _cova_agent(c).s36a_call_count(COVA_GOLFER_PANEL_FN)) > n0:
                    return True
        yield from c.key(0x1B); yield from c.sleep(2000)
    return False


def cov_a_golfer(c):
    """mainLoop H1 / H2 with golfers on the course: the golfer info panel's four action buttons ([0x00824130] =
    0..4 from drawGolferInfoPanel 0x0045c560, consumed by the switch 0x00421a88 at 0x0041e6f9 on the next left click),
    '?' on a selected golfer (0x0041d938), Ctrl / Shift clicks (GetAsyncKeyState at 0x0041ed39 / 0x0041ef79), the
    golfer strip of mode 5 (space, 0x0041eefb..) and the screen-edge camera pan (0x0041e184..0x0041e1ab) at the four
    camera rotations."""
    yield from _cova_to_sandbox(c)
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from _cova_hole(c, (300, 200), (520, 330))
    yield from _cova_hole(c, (570, 380), (330, 280))
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from c.checkpoint("holes")
    yield from _cova_season(c, 200)
    yield from c.checkpoint("season")
    for b, (bx, by) in enumerate(COVA_PANEL_BUTTONS):
        ok = yield from _cova_open_golfer(c)
        yield from c.checkpoint(f"panel_{b}")
        if ok:
            yield from c.click(bx, by); yield from c.sleep(2000)
            yield from c.checkpoint(f"button_{b}")
            yield from c.click(753, 213); yield from c.sleep(2500)   # the character sheet's check mark (button 0 opens it)
            yield from c.click(450, 300); yield from c.sleep(2000)   # the pending action runs on the next left click
            yield from c.click(500, 350); yield from c.sleep(2000)
        yield from _cova_esc(c, 2)
        yield from c.checkpoint(f"action_{b}")
    ok = yield from _cova_open_golfer(c)                            # a golfer selected ([0x53df54] > 0): '?'
    yield from c.click(592, 258); yield from c.sleep(1000)
    yield from _cova_char(c, "?", settle=3000)
    yield from c.checkpoint("q_editor")
    yield from c.click(753, 213); yield from c.sleep(3000)     # the editor's check mark (ui_tour)
    yield from _cova_esc(c)
    yield from c.checkpoint("q_done")
    labels = _cova_obs(c, "labels2", lambda: find_golfer_labels(c.run.g.shot())[:3])
    for vk in (0x11, 0x10):                                    # Ctrl, then Shift, held over a click near a golfer
        for x, y in labels[:2]:
            c.act(0, k="vkey", vk=vk, st=2); yield from c.sleep(200)
            yield from c.click(x, y + 14); yield from c.sleep(1500)
            c.act(0, k="vkey", vk=vk, st=1); yield from c.sleep(500)
            yield from _cova_esc(c, 2)
        yield from c.checkpoint(f"mod_{vk:02x}")
    for (x, y) in ((300, 200), (570, 380)):                    # Shift-click on the two tees (hole markers, bit 0x80)
        c.act(0, k="vkey", vk=0x10, st=2); yield from c.sleep(200)
        yield from c.click(x, y); yield from c.sleep(2500)
        c.act(0, k="vkey", vk=0x10, st=1); yield from c.sleep(500)
        yield from c.checkpoint(f"shift_tee_{x}")
        yield from _cova_esc(c, 2)
    for (x, y) in ((300, 200), (570, 380)):                    # plain clicks on the tees (rename-hole box 0x0041eedb)
        yield from c.click(x, y); yield from c.sleep(2500)
        yield from c.checkpoint(f"tee_{x}")
        for vk in (0x41, 0x42):
            yield from c.key(vk); yield from c.sleep(300)
        yield from c.key(0x0D, char="\r"); yield from c.sleep(2000)
        yield from _cova_esc(c)
    yield from _cova_char(c, " ")                                   # mode 5: the golfer strip along the bottom
    for x in (220, 252, 284, 316):
        yield from c.click(x, 585); yield from c.sleep(1500)
    yield from c.checkpoint("strip")
    yield from _cova_esc(c)
    for rot in range(4):                                       # screen-edge pans at each camera rotation
        for (x, y) in ((0, 300), (799, 300), (400, 0), (400, 599), (0, 0), (799, 599)):
            c.move(x, y); yield from c.sleep(700)
        c.move(400, 300); yield from c.sleep(500)
        yield from _cova_vk(c, 0x27)                                # rotate (report switch: Right)
        yield from c.checkpoint(f"pan_rot{rot}")
    for _ in range(5):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


def cov_a_build2(c):
    """mainLoop H2 second pass: terrain painted over a tee and a green (hole removal 0x00420907 / 0x004208bd), a second
    tee and green for an open hole (0x00420381 message, 0x004202f0), a building painted over (remove prompt 0x00420176,
    declined then accepted), the no-tool right-click mark and demolish (0x0041f1cf..0x0041f20b), the building tools'
    right-click with a tool selected, and Enter / space / Esc tool resets in between."""
    yield from _cova_to_sandbox(c)
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from _cova_hole(c, (300, 200), (520, 330))
    yield from _cova_hole(c, (570, 380), (330, 280))
    yield from c.checkpoint("holes")
    yield from c.click(300, 527); yield from c.sleep(800)        # tee tool: a second tee for the open hole
    yield from c.click(600, 220); yield from c.sleep(1500)
    yield from c.click(330, 200); yield from c.sleep(1500)
    yield from c.click(365, 527); yield from c.sleep(800)        # green tool: a second green, and a green on a tee
    yield from c.click(650, 260); yield from c.sleep(1500)
    yield from c.click(300, 200); yield from c.sleep(1500)
    yield from c.click(300, 350, right=True); yield from c.sleep(1000)
    yield from c.checkpoint("second_tee_green")
    for tool, at in (((428, 527), (300, 200)), ((490, 527), (520, 330)), ((458, 570), (570, 380)), ((552, 527), (330, 280))):
        yield from c.click(*tool); yield from c.sleep(800)     # bunker / fairway / rough / ... over a tee or a green
        yield from c.click(*at); yield from c.sleep(1500)
        yield from c.click(300, 350, right=True); yield from c.sleep(1000)
    yield from c.checkpoint("paint_over_holes")
    yield from c.click(45, 470); yield from c.sleep(1500)        # close; buildings palette
    yield from c.click(118, 495); yield from c.sleep(1500)
    yield from c.click(375, 520); yield from c.sleep(1000)       # first building
    yield from c.click(620, 290); yield from c.sleep(2500)
    yield from c.click(460, 515); yield from c.sleep(1000)       # second building
    yield from c.click(480, 260); yield from c.sleep(2500)
    yield from c.click(620, 290, right=True); yield from c.sleep(1500)   # right click with a tool selected
    yield from c.click(118, 495); yield from c.sleep(1500)
    yield from c.checkpoint("buildings")
    yield from c.click(45, 470); yield from c.sleep(1500)        # terrain tool over the buildings: remove prompt
    yield from c.click(614, 527); yield from c.sleep(800)
    yield from c.click(620, 290); yield from c.sleep(2500)
    yield from c.checkpoint("remove_prompt1")
    yield from _cova_esc(c)                                           # declined
    yield from c.click(480, 260); yield from c.sleep(2500)
    yield from c.checkpoint("remove_prompt2")
    yield from c.key(0x0D, char="\r"); yield from c.sleep(2500)  # accepted (first answer)
    yield from c.click(300, 350, right=True); yield from c.sleep(1000)
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from c.checkpoint("removed")
    for p in ((620, 290), (480, 260), (520, 330)):               # no tool: right click twice = mark, then demolish
        yield from c.click(*p, right=True); yield from c.sleep(1500)
        yield from c.click(*p, right=True); yield from c.sleep(2000)
    yield from c.checkpoint("demolish")
    for ch in "\r \x1b":
        yield from _cova_char(c, ch)
    for _ in range(5):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


def cov_a_keys2(c):
    """mainLoop H1 second pass in a sandbox: space over an open report (0x0041c573), Tab in the terrain-edit grid view
    (0x0041ca04), 'e' in build mode (0x0041cfb6), the hole keys 2 and 4..8 one per poll window, C answered Overwrite
    (0x0041cb73 .. 0x0041cbcc), and 'x' twice."""
    yield from _cova_to_sandbox(c)
    yield from _cova_two_holes(c)
    yield from c.checkpoint("holes")
    yield from _cova_vk(c, 0x70, settle=2500)                         # F1: a report open ([0x569498] != 0)
    yield from _cova_char(c, " ", settle=2000)
    yield from _cova_esc(c, 2)
    yield from c.checkpoint("report_space")
    yield from c.click(118, 495); yield from c.sleep(1500)       # buildings palette -> terrain edit (grid, flag 2)
    yield from c.click(237, 527); yield from c.sleep(1500)
    for _ in range(4):
        yield from _cova_char(c, "\t")
    c.move(420, 300); yield from c.sleep(800)
    for ch in "=-=-":
        yield from _cova_char(c, ch)
    yield from c.click(118, 495); yield from c.sleep(1500)
    yield from c.checkpoint("tab_brush")
    yield from c.click(45, 470); yield from c.sleep(1500)        # build mode, then 'e' twice
    yield from _cova_char(c, "e")
    yield from _cova_char(c, "e")
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from c.checkpoint("e_build")
    for ch in "2456782":                                         # Esc after each: an existing hole's key opens the
        yield from _cova_char(c, ch, settle=2500)                     # hole stats panel, which took the next key
        yield from _cova_esc(c)
    yield from c.checkpoint("holes_keys")
    yield from _cova_char(c, "C", settle=3000)
    yield from c.checkpoint("C_prompt")
    yield from c.click(230, 202); yield from c.sleep(4000)       # "Overwrite the old version."
    yield from c.checkpoint("C_overwrite")
    yield from _cova_esc(c, 2)
    for ch in "xx":
        yield from _cova_char(c, ch)
    for _ in range(5):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


def cov_a_play(c):
    """mainLoop H1 in a practice match (Gary Golf on the course, flag 0x200000 clear): the people palette's pro tab
    starts it (ui_tour's skill-point steps), then on Gary's turn (mode 3) the shot keys f / s / l / h / d, and n (the
    cancel-match prompt 0x0041c71a: Esc, then Yes -> the per-golfer reset loop 0x0041c78e)."""
    yield from _cova_to_sandbox(c)
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from _cova_hole(c, (300, 200), (520, 330))
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from c.checkpoint("hole")
    yield from c.click(175, 535); yield from c.sleep(2500)       # people palette, pro tab, skill points (ui_tour)
    yield from c.click(256, 511); yield from c.sleep(2500)
    yield from c.click(313, 520); yield from c.sleep(2500)
    for _ in range(3):
        yield from c.click(265, 97); yield from c.sleep(600)
    yield from c.click(541, 333); yield from c.sleep(2500)
    yield from c.click(310, 286); yield from c.sleep(2000)
    yield from c.click(541, 333); yield from c.sleep(2500)
    yield from c.click(400, 263); yield from c.sleep(4000)       # "Yea, I don't need ..." -> the match starts
    yield from c.checkpoint("match")
    ok = yield from _cova_wait_gary(c)
    _cova_obs(c, "gary_found", lambda: ok)
    yield from c.checkpoint("gary_turn")
    for ch in "fslhd":
        yield from _cova_char(c, ch, settle=1200)
    yield from c.checkpoint("shot_keys")
    yield from _cova_char(c, "n", settle=3000)                        # cancel-match prompt: keep playing
    yield from c.checkpoint("n_prompt")
    yield from _cova_esc(c, 2)
    yield from _cova_char(c, "n", settle=3000)
    yield from c.click(275, 202); yield from c.sleep(4000)       # "Yes, cancel."
    yield from c.checkpoint("n_yes")
    for _ in range(8):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


# amenity sub-panels (cov_a_probe5 frames): the icon row at y 577 opens a panel of items above it
COVA_AMENITY_SUBS = [
    ((470, 577), [(440, 490), (540, 490), (515, 515)]),                     # benches / ball washers
    ((532, 577), [(480, 500), (580, 495), (560, 525)]),                     # bridges (type 0x11 water only)
    ((594, 577), [(430, 490), (580, 490), (730, 490), (545, 515)]),         # landmarks / statues
    ((655, 577), [(430, 490), (530, 490), (630, 490), (480, 515)]),         # flower beds
    ((717, 577), [(610, 490), (710, 490), (680, 520)]),                     # trees
]


COVA_WATER_SPOTS = [(345, 345), (420, 395), (300, 330)]


def cov_a_build3(c):
    """mainLoop H2 object tools of the amenity sub-panels (clickAmenityPanel 0x00434ae5..0x00434c4d sets [0x4c2854] to
    0, 1, 2, 3, 4, 0x10, 0x13): each sub-panel's items placed on grass, the bridges also on and off the river (t 19,
    type 0x11 only, 0x0041f63a), and the cart-path strip."""
    yield from _cova_to_sandbox(c)
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from c.click(235, 527); yield from c.sleep(2000)
    k = 0
    for si, (sub, items) in enumerate(COVA_AMENITY_SUBS):
        for ii, it in enumerate(items):
            yield from c.click(*sub); yield from c.sleep(1500)
            yield from c.click(*it); yield from c.sleep(1000)
            p = COVA_WATER_SPOTS[ii % len(COVA_WATER_SPOTS)] if si == 1 else COVA_SPOTS[k % len(COVA_SPOTS)]
            k += 1
            yield from c.click(*p); yield from c.sleep(2000)
            if si == 1:
                yield from c.click(*COVA_SPOTS[k % len(COVA_SPOTS)]); yield from c.sleep(1500)   # a bridge off the water
            yield from c.click(300, 350, right=True); yield from c.sleep(1000)
        yield from c.checkpoint(f"amen_sub{si}")
    yield from c.click(330, 505); yield from c.sleep(1000)      # the cart-path strip, dragged
    yield from _cova_drag(c, (480, 260), (600, 320), steps=8)
    yield from c.click(300, 350, right=True); yield from c.sleep(1000)
    yield from c.checkpoint("path")
    for _ in range(5):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


def cov_a_career(c):
    """mainLoop H1 in a career game ([0x822c88] == 0: Start New Game -> Easy -> buy Northeast, $50,000): the routing
    render's career arm (0x0041d2f2) and one game year to the date test 0x0041c17b ([0x834170] == 0x2200, the career-end
    prompt 0x0041c1df..0x0041c322 when the champion's name 0x4c60a4 equals the player's 0x4d6098)."""
    yield from c.wait_screen(imgcmp.load(GOLDEN / "main_menu.png"), name="main_menu")
    yield from c.sleep(3000)
    yield from c.checkpoint("menu")
    yield from c.click(560, 110); yield from c.sleep(3000)      # Start New Game
    yield from c.click(390, 125); yield from c.sleep(4000)      # Easy
    yield from c.click(492, 474)                                # Northeast's icon ($50,000, available on this boot)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "ingame_hud_dial.png"), crop=DIAL, threshold=30, name="hud")
    yield from c.sleep(3000)
    yield from c.checkpoint("game")
    yield from _cova_esc(c)
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from _cova_hole(c, (580, 250), (470, 300))
    yield from c.click(45, 470); yield from c.sleep(1500)
    yield from c.checkpoint("hole")
    yield from _cova_char(c, ".", settle=4000)                       # routing render, career arm ([0x822c88] == 0)
    yield from c.click(400, 300); yield from c.sleep(2000)
    yield from _cova_esc(c)
    yield from c.checkpoint("render")
    t = 0
    while t < 1_000_000:                                        # one game year (~10 date units per virtual second)
        d = _cova_obs(c, "date", lambda: _cova_agent(c).s36a_read_s32(0x00834170))
        if d >= 0x21C0:                                         # close to 0x2200: stop clearing popups with Esc
            break
        yield from c.key(0x1B)
        yield from c.sleep(1800)
        t += 2000
        if t % 100_000 == 0:
            yield from c.checkpoint(f"year_{t // 1000:03d}")
    for k in range(40):
        d = _cova_obs(c, "date2", lambda: _cova_agent(c).s36a_read_s32(0x00834170))
        if d >= 0x2200:
            break
        yield from c.sleep(1000)
    yield from c.sleep(2000)
    yield from c.checkpoint("career_popup")
    yield from c.click(225, 199); yield from c.sleep(3000)      # "I'd like to customize my character." (answer 0)
    yield from c.checkpoint("career_editor")
    yield from c.click(753, 213); yield from c.sleep(3000)      # the editor's check mark
    for k in range(4):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


def _cova_load_row(c, row, title_menu, esc=True):
    """Load the saved game in list row `row` (the list shows &AutoSave1/2 first, then the files sorted by name). No Esc
    afterwards with esc=False: the Esc case 0x0041c9c4 clears flags 0x8000000 / 0x2 (0x0041c9db) and with 0x8000 set
    loads "Tutorial Save.sve" (0x0041c0f9)."""
    if title_menu:
        yield from c.click(170, 80); yield from c.sleep(3000)   # Continue Saved Game
    else:
        yield from _cova_char(c, "L", settle=3000)                    # loadGameBrowserScreen(0, 0) at 0x0041cc72
    yield from c.click(400, 121 + 16 * row); yield from c.sleep(1000)
    yield from c.click(632, 553); yield from c.sleep(8000)
    yield from c.wait_screen(imgcmp.load(GOLDEN / "ingame_hud_dial.png"), crop=DIAL, threshold=30, name=f"hud_row{row}")
    yield from c.sleep(2000)
    if esc:
        yield from _cova_esc(c, 3)


def cov_a_flags(c):
    """mainLoop H1 / H2 blocks gated on flag bits of [0x59e7b8] that no instruction sets (log/c3/c3fya/flagset.py,
    imm.py) and that only a loaded save carries (loadGame 0x0040bb08 stores the saved word): mood5.sve itself (0x1000000:
    X resets the tables, 0x0041ca30), and three copies with one bit added (log/c3/c3fya_run.py copies them in):
    ztut.sve 0x8000 (any key -> showTutorialText 0x0041c147; Esc -> loadGame("Tutorial Save.sve") 0x0041c0f9),
    zdemo.sve 0x8000000 (a left click demolishes, 0x0041eccb), zedit.sve 0x2000000 (stepCourseEditQueue every frame,
    0x0041c593). List rows: 0, 1 autosaves, 2 mood5, 3 stories5, 4 stories9, 5 zdemo, 6 zedit, 7 ztut."""
    yield from c.wait_screen(imgcmp.load(GOLDEN / "main_menu.png"), name="main_menu")
    yield from c.sleep(3000)
    yield from c.checkpoint("menu")
    yield from _cova_load_row(c, 2, True)                             # mood5: X with 0x1000000
    yield from c.checkpoint("mood5")
    yield from _cova_char(c, "X", settle=3000)
    yield from c.checkpoint("mood5_X")
    for p in ((480, 300), (560, 260), (420, 360)):              # X sets flag 0x800 (0x0041ca8b): P6 0x004197aa makes
        yield from c.click(*p); yield from c.sleep(2500)        # [0x4c2854] = 15 (the clubhouse); a click places it
    yield from c.checkpoint("clubhouse")
    yield from _cova_load_row(c, 7, False, esc=False)                 # ztut
    yield from c.checkpoint("ztut")
    yield from _cova_char(c, "z", settle=2500)
    yield from _cova_esc(c)
    yield from c.checkpoint("ztut_esc")
    for _ in range(4):
        yield from c.sleep(1000)
    yield from c.checkpoint("ztut_after")
    yield from _cova_load_row(c, 5, False, esc=False)                 # zdemo
    yield from c.checkpoint("zdemo")
    for p in ((400, 300), (480, 260), (560, 330)):
        yield from c.click(*p); yield from c.sleep(2000)
    yield from c.checkpoint("zdemo_clicks")
    yield from _cova_load_row(c, 6, False)                            # zedit
    yield from c.checkpoint("zedit")
    for _ in range(10):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")

SCENARIOS["cov_a_keys"] = {"env": {}, "phases": [], "lockstep": cov_a_keys}
SCENARIOS["cov_a_build"] = {"env": {}, "phases": [], "lockstep": cov_a_build}
SCENARIOS["cov_a_quit"] = {"env": {}, "phases": [], "lockstep": cov_a_quit}
SCENARIOS["cov_a_quit_title"] = {"env": {}, "phases": [], "lockstep": cov_a_quit_title}
SCENARIOS["cov_a_champ"] = {"env": {}, "phases": [], "lockstep": cov_a_champ}
SCENARIOS["cov_a_build2"] = {"env": {}, "phases": [], "lockstep": cov_a_build2}
SCENARIOS["cov_a_keys2"] = {"env": {}, "phases": [], "lockstep": cov_a_keys2}
SCENARIOS["cov_a_golfer"] = {"env": {}, "phases": [], "lockstep": cov_a_golfer}
SCENARIOS["cov_a_play"] = {"env": {}, "phases": [], "lockstep": cov_a_play}
SCENARIOS["cov_a_build3"] = {"env": {}, "phases": [], "lockstep": cov_a_build3}
SCENARIOS["cov_a_career"] = {"env": {}, "phases": [], "lockstep": cov_a_career}
SCENARIOS["cov_a_flags"] = {"env": {}, "phases": [], "lockstep": cov_a_flags}


def cov_a_keys3(c):
    """mainLoop H1 third pass: '?' while a golfer is selected ([0x53df54] > 0, 0x0041d938), with the golfer info panel
    left open (closing it clears the selection)."""
    yield from _cova_to_sandbox(c)
    yield from _cova_two_holes(c)
    yield from c.checkpoint("holes")
    yield from _cova_season(c, 200)
    yield from c.checkpoint("season")
    ok = yield from _cova_open_golfer(c)
    _cova_obs(c, "panel_ok", lambda: ok)
    yield from c.checkpoint("panel")
    yield from _cova_char(c, "?", settle=3000)                        # panel still open: [0x53df54] = golfer + 1
    yield from c.checkpoint("q_editor")
    yield from c.click(753, 213); yield from c.sleep(3000)       # the editor's check mark
    yield from _cova_esc(c, 2)
    for _ in range(5):
        yield from c.sleep(1000)
    yield from c.checkpoint("end")


SCENARIOS["cov_a_keys3"] = {"env": {}, "phases": [], "lockstep": cov_a_keys3}


# ---- c3fyk2: mutant-kill scenarios for mainLoop 0x0040f5c0 (2026-10-17) ---------------------------------------------
# Each cov_k2_* scenario makes one or more surviving c3fyc mutants (re/tools/c3fy_mutants.tsv) see a differing value,
# mostly by memory injection at a fixed virtual time (values read in the RECORD run, kept in the schedule "obs", written
# identically in every arm). Runner log/c3/c3fyk2_run.py (instances/c3fyk2, outputs log/c3/alt/c3fyk2/<scenario>/);
# notes log/c3/c3fyk2_notes.txt (each injection with the instructions it targets).

COV_K2_GOLFERS = 0x005794C0          # golfer records, 0x100 bytes; world x / y at -8 / -4, screen x / y at +0 / +4


def _cov_k2_rd(c, va):
    return int(c.run.g.scripts[-1].exports_sync.s36a_read_s32(va)) & 0xFFFFFFFF


def _cov_k2_rmw(c, va, name, fn):
    """Read-modify-write of one dword: the value is read in the RECORD run (obs) and fn(old) is written in every arm."""
    v = _cov_b_obs(c, name, lambda: _cov_k2_rd(c, va))
    c.write32(va, fn(v) & 0xFFFFFFFF, tag=name)


def _cov_k2_set_byte(c, va, value, name):
    base, sh = va & ~3, (va & 3) * 8
    _cov_k2_rmw(c, base, name, lambda v: (v & ~(0xFF << sh)) | ((value & 0xFF) << sh))


def _cov_k2_set_word(c, va, value, name):
    base, sh = va & ~3, (va & 3) * 8
    _cov_k2_rmw(c, base, name, lambda v: (v & ~(0xFFFF << sh)) | ((value & 0xFFFF) << sh))


def _cov_k2_sandbox_hole(c, tee=(300, 200), green=(420, 260), open_hole=True):
    yield from _cov_b_menu(c)
    yield from _cov_b_sandbox(c, "monterey")
    yield from c.click(45, 470); yield from c.sleep(1200)
    yield from c.click(300, 527); yield from c.sleep(800)
    yield from c.click(*tee); yield from c.sleep(1200)
    yield from c.click(365, 527); yield from c.sleep(800)
    yield from c.click(*green); yield from c.sleep(2000)
    yield from c.click(300, 350, right=True); yield from c.sleep(1500)
    if open_hole:
        yield from c.key(0x48); yield from c.sleep(3000)
        yield from c.key(0x1B); yield from c.sleep(1500)
        yield from c.click(45, 470); yield from c.sleep(1200)


def _cov_k2_active_golfers(c):
    """(index, byte +0x19, screen x, partner word +0x9a) of every golfer record whose byte +0x19 is neither 0 nor 0xff
    (the per-golfer pass 0x0041497b / 0x00414983)."""
    out = []
    for i in range(152):
        b = COV_K2_GOLFERS + i * 0x100
        a = (_cov_k2_rd(c, b + 0x18) >> 8) & 0xFF
        if a in (0, 0xFF):
            continue
        sx = _cov_k2_rd(c, b)
        out.append([i, a, sx if sx < 0x80000000 else sx - 0x100000000, _cov_k2_rd(c, b + 0x98) >> 16])
    return out


def cov_k2_golfers(c):
    """Survivors 209, 202 and 210 (golfer pass P3 / comment text P4). Sandbox, one hole, 80 virtual s of play, then the
    simulation is paused with 'P' (0x0041d020 xors bit 2 of g_flags; P5_sim returns at 0x00417603 before updateGolfers), so
    golfer positions stay where the injections put them while P3 / P4 keep running every frame.
      209 (0x00416b4b jg): golfer i (even) and its partner i + 1 at the same world point (equal screen y), both with comment
          byte +0x7c = 3 and comment id +0x7d = 1 (not 0x32: that arm recomputes ebx at 0x00416b92): the original takes
          `ebx += 4` (0x00416b56: index even), the mutant `ebx -= 4`; the text y (0x00416dcf) moves 8 px.
      202 (0x004151dc jge): golfer j in state 0x12 (+0x15) with word +0x96 = -1: 0x004151b6 passes (-1 > 0xc - [counts
          0x3c + ebx], counts ~20), eax = 0xb + 1 = 0xc: the original keeps state 0x12 (jge), the mutant sets 0xb and
          snaps the world point (0x004151de..0x0041520d).
      210 (0x00416b0e movsx): golfer i's partner word +0x9a = 0xffff: the original reads golfer record -1 (edi = -0x100),
          the mutant edi = 0xffff00 (0x0157943c)."""
    yield from _cov_k2_sandbox_hole(c)
    yield from _cov_b_season(c, 80000)
    yield from _cova_char(c, "P")                                      # WM_CHAR alone (see _cova_char)
    yield from c.checkpoint("paused")
    act = _cov_b_obs(c, "active", lambda: _cov_k2_active_golfers(c))
    ids = {g[0]: g for g in act}
    pair = next((g[0] for g in act if g[0] % 2 == 0 and g[0] + 1 in ids and g[2] != -1 and g[3] == g[0] + 1), None)
    if pair is not None:
        a, b = COV_K2_GOLFERS + pair * 0x100, COV_K2_GOLFERS + (pair + 1) * 0x100
        for off, nm in ((-8, "wx"), (-4, "wy")):
            v = _cov_b_obs(c, f"g{pair}_{nm}", lambda off=off: _cov_k2_rd(c, a + off))
            c.write32(b + off, v, tag=f"golfer {pair + 1} {nm} = golfer {pair}")
        for g in (a, b):
            _cov_k2_rmw(c, g + 0x7c, f"comment_{g:x}", lambda v: (v & 0xFFFF0000) | 0x0103)
    yield from c.sleep(1500)
    yield from c.checkpoint("same_y")
    j = next((g[0] for g in act if pair is None or g[0] not in (pair, pair + 1)), None)
    if j is not None:
        r = COV_K2_GOLFERS + j * 0x100
        _cov_k2_set_byte(c, r + 0x15, 0x12, f"g{j}_state12")
        _cov_k2_set_word(c, r + 0x96, 0xFFFF, f"g{j}_w96")
    yield from c.sleep(1500)
    yield from c.checkpoint("state12")
    if pair is not None:
        # partner word 0xffff: the original's movsx (0x00416b0e) gives edi = -0x100, so it reads "record -1": comment byte
        # 0x0057943c (0x00416b23) and screen y 0x005793c4 (0x00416b2b); nothing in the image references either address
        # and neither is a .sve field. Make that phantom partner commenting at golfer `pair`'s screen y, so the original
        # moves the text by 4 px (0x00416b56, index even) while the mutant's 0x0157943c byte decides alone.
        _cov_k2_set_word(c, COV_K2_GOLFERS + pair * 0x100 + 0x9a, 0xFFFF, f"g{pair}_partner")
        sy = _cov_b_obs(c, f"g{pair}_sy", lambda: _cov_k2_rd(c, COV_K2_GOLFERS + pair * 0x100 + 4))
        _cov_k2_set_byte(c, 0x0057943C, 3, "rec_m1_comment")
        c.write32(0x005793C4, sy, tag="rec_m1_sy")
    yield from c.sleep(1500)
    yield from c.checkpoint("partner")


SCENARIOS["cov_k2_golfers"] = {"env": {}, "phases": [], "lockstep": cov_k2_golfers}

COV_K2_FAIRWAY = (330, 570)          # Build Course palette slot that paints tile kind 2 (k2x_kinds probe)
COV_K2_BULLDOZE = (770, 565)         # the slot whose click runs the kind-0xf tile animation that plays sound 0x92
COV_K2_OBJECTS = 0x0058BCB8          # placed objects, 16 bytes: +0 s16 type, +2 x, +4 y, +6 flags, +8 buyer, +0xc value


def _cov_k2_lot_dates(c, k, after):
    """The next two dates > after at which P5_dateTick processes placed object k: date % (0x400 / (mode + 2)) == 0
    (0x00417835..0x00417840) and (date / that) % (mode + 2) == k % (mode + 2) (0x004179e1..0x004179f6)."""
    mode = _cov_b_obs(c, "mode", lambda: _cov_k2_rd(c, 0x00822C88))
    e = mode + 2
    per = 0x400 // e
    d, out = (after // per + 1) * per, []
    while len(out) < 2:
        if (d // per) % e == k % e:
            out.append(d)
        d += per
    return mode, out


def cov_k2_build(c):
    """Survivors 219, 204, 203, 225 and 232 on a sandbox course.
      219 (0x00417342 skipped store): the build HUD 0x00417277 stores the yardage word +4 of the hole record (a .sve
          field) only when P3's shot preview left it 0 (0x0041391e, set at 0x00413954 when the preview reaches the green)
          and re-stores it above 200 yards (0x00417349). With g_flags bit 0x40000 set (0x0041390e, every frame) the
          preview reads its landing tiles from the cache 0x00542dfc / 0x00542e24 (0x004136ef); a tree tile written there
          (attribute 4 > 1, 0x00413740) stops it short of the green, so on this 103-yard hole the HUD store runs and the
          mutant leaves the word 0. The cache is not a .sve field and is rewritten when a build action clears the bit.
      204 (0x00413d04 skipped store): the pin search runs only for radii 150 / 200 / 250 below the preview's path length in
          yards (0x00413be6), so hole 2 is a 232-yard hole, with fairway (tile kind 2, 0x00413c63, neighbour tests
          0x00413cab..0x00413cee) painted on its approach side: the search stores x into the .sve table 0x0059ae80 (the
          mutant keeps the -1 the rep stosd at 0x00413b93 wrote).
      203 (0x004147a1 push 0x32): the bulldoze slot runs the kind-0xf tile animation whose frame 0 calls playSound(0x92,
          0x32) (0x004147a8); Snd::setVolume 0x00485140 keeps the volume in m_4 of sound 0x92 (0x008115dc), a region
          log/c3/c3fyk2_run.py adds to the compared state for cov_k2_* scenarios only.
      225 (0x00417e75 movsx): word 0 of hole 1's 64-word statistic (0x00575d90) set to 0x8008, then the date is moved to
          48 ticks before the month end, so the 1/8 decay 0x00417e75..0x00417e8a runs on a negative word (0x9007 after).
          The mutant as generated assigns its unsigned load to the `short s` of P5_monthly, so it computes the same value
          (equivalent; kept here as the run that shows the site executing with a negative word).
      232 (0x00417abf jg): a type-5 record (a home-site lot) injected into a free slot of the placed-object table; its
          first processing measures rateLot (value 0 -> rate); then the value v with rate + v - v / 12 == lim
          ((nb * 300 + 400) * (mode + 2), 0x00417aa3..0x00417abc) is written before the next processing: the original
          keeps the lot unsold (not >), the mutant sells it (startMessage, buyer, spawnObject)."""
    yield from _cov_b_menu(c)
    yield from _cov_b_sandbox(c, "monterey")
    yield from c.click(45, 470); yield from c.sleep(1200)
    yield from c.click(300, 527); yield from c.sleep(800)
    yield from c.click(300, 200); yield from c.sleep(1200)
    yield from c.click(365, 527); yield from c.sleep(800)
    yield from c.click(420, 260); yield from c.sleep(2000)
    yield from c.click(300, 350, right=True); yield from c.sleep(1500)
    yield from c.checkpoint("built")
    # 219: the preview cache (10 landing tiles per table) -> tree tile (38, 22) of the Monterey map
    for i in range(10):
        c.write32(0x00542DFC + 4 * i, 38, tag=f"preview cache x[{i}] = tree")
        c.write32(0x00542E24 + 4 * i, 22, tag=f"preview cache y[{i}] = tree")
    yield from c.sleep(1500)
    yield from c.checkpoint("cached_miss")
    yield from c.key(0x48); yield from c.sleep(3000)                  # open hole 1
    yield from c.key(0x1B); yield from c.sleep(1500)
    # 204: hole 2, 232 yards (tee tile (36, 12), green (37, 21) on this view), fairway on its approach side
    yield from c.click(300, 527); yield from c.sleep(800)
    yield from c.click(186, 181); yield from c.sleep(1200)
    yield from c.click(365, 527); yield from c.sleep(800)
    yield from c.click(490, 333); yield from c.sleep(2000)
    yield from c.click(300, 350, right=True); yield from c.sleep(1500)
    yield from c.click(*COV_K2_FAIRWAY); yield from c.sleep(800)
    for tx in range(35, 40):
        for ty in range(16, 21):
            yield from c.click(30 * (tx + ty) - 1250, 19 * (ty - tx) + 637); yield from c.sleep(300)
    yield from c.click(300, 350, right=True); yield from c.sleep(2000)
    yield from c.checkpoint("fairway")
    yield from c.click(*COV_K2_BULLDOZE); yield from c.sleep(800)
    yield from c.click(698, 330); yield from c.sleep(1500)
    yield from c.click(698, 330, right=True); yield from c.sleep(1500)
    yield from c.checkpoint("bulldoze")
    yield from c.key(0x48); yield from c.sleep(3000)                  # open hole 2
    yield from c.key(0x1B); yield from c.sleep(1500)
    yield from c.click(45, 470); yield from c.sleep(1200)
    yield from _cov_b_season(c, 20000)
    yield from c.checkpoint("opened")
    # 225: a negative statistic word, then the month end
    _cov_k2_set_word(c, 0x00575D90, 0x8008, "hole1_stat0")
    d = _cov_b_obs(c, "date225", lambda: _cov_k2_rd(c, COV_B_DATE))
    c.write32(COV_B_DATE, (d | 0x3FF) - 0x30, tag="date -> 48 before the month end")
    yield from _cov_b_season(c, 8000)
    yield from c.checkpoint("decay")
    # 232: a home-site lot in the first free placed-object slot
    k = _cov_b_obs(c, "free_slot", lambda: next(i for i in range(1, 256)
                                                 if _cov_k2_rd(c, COV_K2_OBJECTS + 16 * i) & 0xFFFF == 0xFFFF))
    rec = COV_K2_OBJECTS + 16 * k
    c.write32(rec + 8, 0, tag="lot buyer 0")
    c.write32(rec + 0xc, 0, tag="lot value 0")
    c.write32(rec + 4, 30, tag="lot y 30, flags 0")
    c.write32(rec, (30 << 16) | 5, tag="lot type 5 at x 30")
    d = _cov_b_obs(c, "date232", lambda: _cov_k2_rd(c, COV_B_DATE))
    mode, (d1, d2) = _cov_k2_lot_dates(c, k, d + 0x40)
    c.write32(COV_B_DATE, d1 - 10, tag="date -> 10 before the lot's processing")
    yield from c.sleep(3000)
    rate = _cov_b_obs(c, "lot_rate", lambda: _cov_k2_rd(c, rec + 0xc))
    nb = _cov_b_obs(c, "houses", lambda: _cov_k2_rd(c, 0x004C284C))
    lim = (nb * 300 + 400) * (mode + 2)
    want = lim - rate
    v = next(x for x in range(0, 2 * want + 24) if x - x // 12 == want) if want >= 0 else 0
    _cov_b_obs(c, "lot_plan", lambda: {"k": k, "mode": mode, "d1": d1, "d2": d2, "rate": rate, "lim": lim, "v": v})
    c.write32(rec + 8, 0, tag="lot buyer 0")
    c.write32(rec + 0xc, v, tag="lot value: rate + v - v / 12 == lim")
    c.write32(COV_B_DATE, d2 - 10, tag="date -> 10 before the lot's next processing")
    yield from c.sleep(3000)
    yield from c.checkpoint("lot")
    yield from _cov_b_season(c, 6000)
    yield from c.checkpoint("end")


SCENARIOS["cov_k2_build"] = {"env": {}, "phases": [], "lockstep": cov_k2_build}


def cov_k2_label(c):
    """Survivor 218 (0x004167d5 push 0xa): a new career (Easy, Monterey) shows the tutorial label at walker 0 (state 0xfa,
    P4 0x00416305..0x00416838; drawn while date & 0x18 != 0, 0x00416495). Its band is drawMapLine 0x004493d0 ->
    Terrain::drawLine with width 0xa; Terrain.dll's pixel format (0x10063e10: PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL,
    16 bits) runs on Microsoft's GDI Generic OpenGL 1.1. Terrain::initGL 0x100033e0 enables GL_LINE_SMOOTH (GL_NICEST)
    and nothing disables it; GDI Generic clamps smooth line widths to its range [0.5, 10], so widths 10 and 11 draw the
    same pixels and this scenario reads SAME for the mutant with 0x004167d5 reached at every checkpoint: it is the
    equivalence evidence for 218 (log/c3/c3fyk2_notes.txt), not a kill. Checkpoints every 0.7 virtual s."""
    yield from _cov_b_menu(c)
    yield from c.click(560, 110); yield from c.sleep(3000)      # Start New Game
    yield from c.click(390, 125); yield from c.sleep(8000)      # Easy
    yield from c.click(*COV_B_LOC["monterey"])                   # Phoenix costs more than the $100,000 start here
    yield from _cov_b_hud(c)
    yield from c.sleep(4000)
    yield from c.checkpoint("career")
    yield from c.key(0x1B); yield from c.sleep(1500)            # the welcome text
    for i in range(5):
        yield from c.sleep(700)
        yield from c.checkpoint(f"label{i}")


SCENARIOS["cov_k2_label"] = {"env": {}, "phases": [], "lockstep": cov_k2_label}

COV_K2_CAM = (0x004C2BA0, 0x004C2BA4)   # camera tile x / y (arrows move it 4 tiles, events.js setCamera 1)


def _cov_k2_pair_xy(c, g):
    a, b = COV_K2_GOLFERS + g * 0x100, COV_K2_GOLFERS + (g ^ 1) * 0x100
    s = lambda v: v - 0x100000000 if v >= 0x80000000 else v
    return [s(_cov_k2_rd(c, a)), s(_cov_k2_rd(c, a + 4)), s(_cov_k2_rd(c, b)), s(_cov_k2_rd(c, b + 4))]


def _cov_k2_place_pair(c, g, cam0, target, coarse, ok, name):
    """Put golfers g and g ^ 1 on one world point (dwords -8 / -4 of their records) and move the camera and that point
    until ok(pair screen points): the camera first, aimed at `coarse` (32 px in x per tile sum, 20 px in y per tile
    difference at zoom 4; P3 adds a height offset after that, so the coarse aim stays inside the edge), then the world point
    in steps of at most `lim` px (x follows wx + wy at 1/32 px per unit, y follows wy - wx at 1/51.2), back to the last
    on-screen point with half the step whenever P3 reports the pair off screen (+0 = -1, 0x00414969). RECORD run only;
    every applied step is kept in obs and the replay arms apply the same steps at the same virtual times."""
    a, b = COV_K2_GOLFERS + g * 0x100, COV_K2_GOLFERS + (g ^ 1) * 0x100

    def apply(st):
        cx, cy, wx, wy = st
        c.write32(COV_K2_CAM[0], cx, tag=f"{name} cam x"); c.write32(COV_K2_CAM[1], cy, tag=f"{name} cam y")
        for r in (a, b):
            c.write32(r - 8, wx & 0xFFFFFFFF, tag=f"{name} world x"); c.write32(r - 4, wy & 0xFFFFFFFF, tag=f"{name} world y")
        yield from c.sleep(300)

    if c.run.mode == "replay":
        res = _cov_b_obs(c, f"{name}_place", lambda: None)
        for st in res["steps"]:
            yield from apply(st)
        return res
    steps, seen = [], []
    wx, wy = _cov_k2_rd(c, a - 8), _cov_k2_rd(c, a - 4)
    st = [cam0[0], cam0[1], wx, wy]
    steps.append(st); yield from apply(st)
    got = _cov_k2_pair_xy(c, g); seen.append(got)
    s_, d_ = round((got[0] - coarse[0]) / 32), round((coarse[1] - got[1]) / 20)
    if (s_ + d_) % 2:
        d_ -= 1 if coarse[1] > got[1] else -1
    st = [cam0[0] + (s_ + d_) // 2, cam0[1] + (s_ - d_) // 2, wx, wy]
    steps.append(st); yield from apply(st)
    got = _cov_k2_pair_xy(c, g); seen.append(got)
    good, lim = (st, got) if got[0] != -1 else (None, 12), 12
    for _ in range(24):
        if got[0] != -1 and ok(got):
            break
        if got[0] == -1:
            if good is None:
                break
            st, got = good
            lim = max(1, lim // 2)
            steps.append(st); yield from apply(st)
            got = _cov_k2_pair_xy(c, g); seen.append(got)
            continue
        good = (st, got)
        dx = max(-lim, min(lim, target[0] - got[0]))
        dy = max(-lim, min(lim, target[1] - got[1]))
        su, tu = 32 * dx, 51 * dy
        st = [st[0], st[1], st[2] + (su - tu) // 2, st[3] + (su + tu) // 2]
        steps.append(st); yield from apply(st)
        got = _cov_k2_pair_xy(c, g); seen.append(got)
    res = {"steps": steps, "seen": seen, "final": got, "ok": got[0] != -1 and ok(got)}
    _cov_b_obs(c, f"{name}_place", lambda: res)
    return res


def cov_k2_simfoto(c):
    """Survivors 222 and 228 (P5 SimFoto box, 0x004185bb..0x00418b02). The box is drawn around golfer g and its partner
    g ^ 1 while [0x004c2e0c] holds g (low byte); with bit 0x100 (the form matchUpdate stores at 0x00427546 / 0x00428505)
    the box is 0xa0 x 0x78 (0x00418642..0x00418667). The golfers' screen points come from P3's worldToScreen with margin 0
    (0x00415395 / 0x0041539b: 0 <= x < 800, 0 <= y < 600), so the clamp tops 0x2bc / 0x1f4 bind only when the pair is at
    the right / bottom edge: x0 = clamp(min x - (0xa0 - w) / 2 - 0x10, ..) > 0x2bc needs both x >= 794; y0 needs the pair's
    mid y >= ~577. Sandbox, one hole, 80 virtual s, pause ('P', as cov_k2_golfers), then [0x004c2e0c] = g | 0x100 for the
    pair whose x are closest, and the camera tile pair 0x004c2ba0 / 0x004c2ba4 moved (offset solved in the RECORD run from
    the measured per-tile screen steps, verified on the frame, kept in obs) so the pair sits at the bottom edge (222,
    checkpoint `bottom`) and then at the right edge (228, checkpoint `right`)."""
    yield from _cov_k2_sandbox_hole(c)
    yield from _cov_b_season(c, 80000)
    yield from _cova_char(c, "P")
    act = _cov_b_obs(c, "active", lambda: _cov_k2_active_golfers(c))
    ids = {g[0]: g for g in act}
    pairs = [(abs(g[2] - ids[g[0] ^ 1][2]), g[0]) for g in act if g[0] % 2 == 0 and (g[0] ^ 1) in ids
             and g[2] != -1 and ids[g[0] ^ 1][2] != -1]
    g = min(pairs)[1] if pairs else 0
    yield from c.sleep(1500)
    yield from c.checkpoint("paused")
    cam0 = _cov_b_obs(c, "cam0", lambda: [_cov_k2_rd(c, COV_K2_CAM[0]), _cov_k2_rd(c, COV_K2_CAM[1])])
    base = _cov_b_obs(c, "base", lambda: _cov_k2_pair_xy(c, g))
    c.write32(COV_K2_CAM[0], cam0[0] + 1, tag="probe cam x + 1")
    yield from c.sleep(400)
    px = _cov_b_obs(c, "probe_x", lambda: _cov_k2_pair_xy(c, g))
    c.write32(COV_K2_CAM[0], cam0[0], tag="probe cam x back")
    c.write32(COV_K2_CAM[1], cam0[1] + 1, tag="probe cam y + 1")
    yield from c.sleep(400)
    py = _cov_b_obs(c, "probe_y", lambda: _cov_k2_pair_xy(c, g))
    c.write32(COV_K2_CAM[1], cam0[1], tag="probe cam y back")
    yield from c.sleep(400)
    ux = [px[k] - base[k] for k in range(4)]
    uy = [py[k] - base[k] for k in range(4)]

    def on(p):
        return all(0 <= p[k] < 800 for k in (0, 2)) and all(0 <= p[k] < 600 for k in (1, 3))

    def bottom(p):      # 222: ebx = min y - (0x78 - h) / 2 >= 0x205 (+2 for rounding), h < 0x78
        h = abs(p[1] - p[3])
        return on(p) and h < 0x78 and min(p[1], p[3]) - (0x78 - h) // 2 >= 0x207

    def right(p):       # 228: [F+0xc] = min x - (0xa0 - w) / 2 >= 0x2cd (+1), w < 0xa0
        w = abs(p[0] - p[2])
        return on(p) and w < 0xa0 and min(p[0], p[2]) - (0xa0 - w) // 2 >= 0x2ce

    # the camera moves first: while [0x004c2e0c] != -1 the view (and P3's projections) stay as they were (cov_k2_simfoto's
    # first recording: camera writes after the selector moved nothing), so the box uses the edge points
    for name, ok, target, coarse in (("bottom", bottom, (400, 589), (400, 560)), ("right", right, (798, 300), (770, 300))):
        yield from _cov_k2_place_pair(c, g, cam0, target, coarse, ok, name)
        yield from c.sleep(800)
        c.write32(0x004C2E0C, g | 0x100, tag=f"simfoto golfer {g} | 0x100")
        yield from c.sleep(1500)
        yield from c.checkpoint(name)
        # leave the SimFoto as the game does: a camera key reaches 0x0041e253 (selector != -1: playSound, refreshFull,
        # [0x004c2e0c] = -1 at 0x0041e27d, pumpAndRedraw); a plain write of -1 left the view frozen (k2 draft run x2)
        yield from _cova_vk(c, 0x25)


SCENARIOS["cov_k2_simfoto"] = {"env": {}, "phases": [], "lockstep": cov_k2_simfoto}
