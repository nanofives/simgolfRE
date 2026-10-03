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


def build_four_holes(g):
    build_first_hole(g)
    for tee, green in FOUR_HOLES:
        build_next_hole(g, tee, green)


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
