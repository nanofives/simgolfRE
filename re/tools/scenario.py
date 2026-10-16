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
