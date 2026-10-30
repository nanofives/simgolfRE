"""Phase 5 frame gate (roadmap step S0.2): every checkpoint of a pinned scenario rendered by the port, compared by region with the
original's back-buffer frames.

One boot run per scenario (port/test/boot.py, the boot gate's run: the user's exe image, the scenario's trace, the per-frame sim
hashes compared with the capture) with the graphics backend attached (gfx.attach, port/test/gfx.py: one gfx.snapshotAt per
checkpoint at the trace ordinal of the checkpoint's clock call) and LiveTerrain attached (sg_ts_live_attach, port/test/terrainlive.py:
the port's own Terrain.dll state; the Terrain object goes to the address the pin_<scenario> recording's getInstance returned, or, for a
scenario without a recording, to the address LiveTerrain picks itself: 0 = "the first free 4 MB step above the GameState + 32 MB").
The boot's verdict (PASS / KNOWN) with both attached is the proof that neither changes a sim hash: boot.run compares every frame
record with the capture's framehash.bin, as the boot gate does. On the Win32 backend (SG_BACKEND=win32) the runner's ops 28..32
carry the sg_ts_live calls (roadmap S3.2, port/test/terrainlive.py live_*). With LiveTerrain and gfx attached the terrain passes
are rasterised into the backend's root surface (the terrain body family, port/src/sim/gfx_terrain.cpp); gfx.log's
snapshot_terrain gives column terrain_drawn.

One TSV row per checkpoint (columns in COLUMNS):
  status   PASS (0 px differ) / FAIL (px differ, no unsupported drawing call before the snapshot; detail = px by region) /
           UNSUPPORTED (px differ, detail = the first unsupported call of the run) / NOT_RUN (detail = the boot status or why)
  kind     the screen kind that picks the regions: title (the title menu: gfx.menu_regions), location (courseActionMenu), ingame
           (mainLoop's frame: the course view and the HUD panels P6_Hud / P6_StatusBar / P6_PauseLabel / showMainRadialMenu /
           P6_Panels draw), frame (anything else: the whole frame)
  context  where the original was at the checkpoint's clock call, from the trace's clock-call sites between the frame head and the
           checkpoint (classify()): HEAD, H1WAIT, MODAL:<function>, MODAL_AFTER_H1:<function>, AFTER_H1:title | waitTicks,
           BOOT:<function>; wait_ticks = the n of the waitTicks(n, 0) loop it sat in (-1: not in one)
  terrain  yes / no, terrain_rule = the rule's reason (terrain_rule())
  regions  name=px/one_level_px/one_level_pct/beyond_px;...  (each pixel counted in the first region of the kind's list holding it;
           the last region is the rest, so the regions add up to total_px). A pixel differs when any 5-bit channel differs;
           one-level = the largest channel difference is 1 level (8-bit values >> 3), beyond = more than 1; pct = one-level px / region area.
  unsup_before / first_unsupported / first_unsupported_drawing (NOT_DRAWING excluded) / unsupported_before: the distinct unsupported drawing calls (gfx.log reasons) before the snapshot,
           unsup_order = how their order is known (gfx = the backend's own first-occurrence list, probe = the call trace of a second
           run, final = every reason of the run occurred before the snapshot, partial = order not fully known; see order_probe())

    ~/bin/py -3.12 -m port.test.framegate sandbox_basic cov_b_location --out log/roadmap/S0/2_sample.tsv
    ~/bin/py -3.12 -m port.test.framegate --all --jobs 2 --out <tsv> [--resume]          # the census (step S0.3)
    options: --checkpoints a,b  --negctl gfx.attach=1  --swap-ref <chk>=<scenario>/<chk>  --images <dir>  --no-probe  --host-gl
             --gl raster|auto|host (roadmap S3.20, decision G13: the terrain renderer; default raster = GlRaster; opt-in auto = the
             host's OpenGL when the startup self-check of gl_wgl.h passes, else GlRaster; host (or wgl) = the host's OpenGL without
             the check; columns renderer / renderer_reason say what each run drew with)
"""
from __future__ import annotations

import argparse
import bisect
import concurrent.futures as cf
import ctypes as C
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import time

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
for p in (str(ROOT), str(ROOT / "re" / "tools")):
    if p not in sys.path:
        sys.path.insert(0, p)
import trace_format as TF  # noqa: E402
from port.test import boot as B  # noqa: E402
from port.test import gfx as GX  # noqa: E402
from port.test import lib as L  # noqa: E402

REFS_JSON = ROOT / "re" / "analysis" / "data" / "scenario_refs.json"
FUNCS_TSV = ROOT / "re" / "functions_ghidra.tsv"
W, H = GX.W, GX.H
MAX_JOBS = 2
PROBE_LIMIT = 3_000_000      # TkBase calls traced by the order probe (about 60 MB of trace, deleted afterwards)
COLUMNS = ["scenario", "checkpoint", "status", "detail", "kind", "context", "frame", "wait_ticks", "terrain", "terrain_rule", "total_px",
           "one_level_px", "one_level_pct", "beyond_px", "regions", "unsup_before", "first_unsupported", "first_unsupported_drawing", "unsupported_before",
           "unsup_order", "boot_status", "boot_records", "liveterrain", "backend", "variant", "reference", "terrain_drawn",
           "renderer", "renderer_reason"]
GL_MODES = {"raster": 0, "wgl": 1, "host": 1, "auto": 2}   # terrainLive.renderer's argument (terrain_live.cpp, roadmap S3.20)

# The H1 frame wait of mainLoop (re/analysis/sim/time_model.md: 0x0041e631..0x0041e6b1, timeGetTime via [0x004ba21c] then
# waitTicks(1, 0) steps): its own clock reads return inside this range
H1_WAIT = (0x0041E631, 0x0041E6B1)
# Screens drawn over the whole frame that a frame's H1 runs as a modal loop (cov_r_quit's credits: reference frames quit_00..credits)
FULL_SCREEN_MODALS = {"showCreditsScreen"}
# Inside the H1 wait every pass reads the clock at H1_WAIT, then waitTicks(1, 0) reads it 11 times (10 n + 1, wait_ticks()): a
# checkpoint more reads than this past the frame's last H1_WAIT read has left the wait (2_h1_tail.py: 0..11 or >= 2,021 over
# every in-frame checkpoint of the 61 traced scenarios)
H1_WAIT_READS = 11
# Drops gfx.log counts as unsupported although they draw nothing, by their reason in tick.cpp's kDrops table: playSound /
# startSoundSlot "audio request", Snd::status "audio device status query", capsLockOff "keybd_event", emptyFunc_4a0890 "empty
# function", waitTicks "wait loop (pacing)", saveCourseJpg "screenshot file", Widget_readPixel "a pixel read of the root surface",
# enterModal483d30 "modal stack push and focus clear of the window tree". They are listed but never make a row UNSUPPORTED.
NOT_DRAWING = {"playSound", "startSoundSlot", "Snd::status", "capsLockOff", "emptyFunc_4a0890", "waitTicks", "saveCourseJpg",
               "Widget_readPixel", "enterModal483d30"}

# ------------------------------------------------------------------------------------------------ regions per screen kind
# rectangles (x, y, w, h); anchors from the frame code, extents measured on the reference named in the comment
INGAME_REGIONS = [
    # P6_Hud (tick_frame.inc): course image at (3, 3), title strip at (0x30, 5), title / date centred at x 0x90, y 0xd / 0x1b, the
    # counts row at y 0x26; extent to the strip's right edge and the logo's bottom on sandbox_basic/sandbox_00
    ("hud_title", (0, 0, 0xE8, 0x52)),
    # P6_StatusBar: the three framed boxes at (0x289, 0xd), (0x2a5, 0x37), (0x2bb, 0x63) and their texts to (0x2dc, 0x79); the icons
    # reach the right edge (sandbox_basic/sandbox_00)
    ("hud_meters", (0x280, 0, 0xA0, 0x8C)),
    # P6_PauseLabel / the message line drawCenteredText2 draws centred at (0x190, 0xa)
    ("hud_message", (0xFA, 0, 0x12C, 0x1A)),
    # showMainRadialMenu: the base image at (0, 0x1b3), the mode panels to (0x92, 0x1f8) / (0x8e, 0x22b)
    ("toolbar", (0, 0x1B3, 0xD0, 0xA5)),
    # P6_Panels' panel modes (showInfoCursorPanel, drawBuildingPanel, drawEmployeeViewPanel, drawGolferActionPanel) and P6_Hud's
    # golfer strip from x 0xcc at y 0x23f; band measured on cov_a_build/bldg_8 (the build palette)
    ("bottom_panel", (0xD0, 0x1F0, 0x250, 0x68)),
]
LOCATION_REGIONS = [
    # courseActionMenu (saveload.cpp): the two heading lines centred at (0x7e, 0x13) / (0x7e, 0x21)
    ("loc_title", (0x0E, 0x08, 0xE0, 0x28)),
    # the cash text centred at (0x2e4, 0x12)
    ("loc_cash", (0x294, 0x04, 0xA0, 0x1C)),
    # the legend plates at (0x18 | 0x7c | 0xf4, 0x226) and their texts to x 0x108
    ("loc_legend", (0x10, 0x220, 0x110, 0x2C)),
    # the Save / Load / back buttons at (0x2d4, 0x198), (0x2d4, 0x1ca), (0x2dc, 0x214)
    ("loc_buttons", (0x2D0, 0x190, 0x50, 0xC8)),
]


def menu_region_list() -> list:
    regs = GX.menu_regions()
    labels = [(k, v) for k, v in regs.items() if k.startswith("label_")]
    rest = [(k, v) for k, v in regs.items() if not k.startswith("label_")]
    return labels + rest


def regions_for(kind: str) -> list:
    """[(name, (x, y, w, h)), ...] in priority order; the remainder region is added by region_numbers()."""
    return {"title": menu_region_list(), "location": LOCATION_REGIONS, "ingame": INGAME_REGIONS}.get(kind, [])


REST_NAME = {"title": "background", "location": "loc_map", "ingame": "view", "frame": "frame"}


def label_map(kind: str) -> tuple[np.ndarray, list[str]]:
    regs = regions_for(kind)
    names = [n for n, _ in regs] + [REST_NAME.get(kind, "frame")]
    lab = np.full((H, W), len(regs), np.int16)
    for i in range(len(regs) - 1, -1, -1):        # first listed wins: paint in reverse
        x, y, w, h = regs[i][1]
        lab[max(0, y):min(H, y + h), max(0, x):min(W, x + w)] = i
    return lab, names


def region_numbers(port: np.ndarray, ref: np.ndarray, kind: str) -> dict:
    """Per region: px differing, one-level px, one-level % of the region's area, px beyond one level."""
    a, b = port.astype(np.int16) >> 3, ref.astype(np.int16) >> 3     # 5-bit levels (the frames are RGB555 expanded to 8 bits)
    d = np.abs(a - b).max(axis=2)
    lab, names = label_map(kind)
    out = {}
    for i, n in enumerate(names):
        m = lab == i
        area = int(m.sum())
        px = int((d[m] > 0).sum())
        one = int((d[m] == 1).sum())
        out[n] = {"px": px, "one": one, "pct": round(100.0 * one / area, 3) if area else 0.0, "beyond": px - one, "area": area}
    return out


def fmt_regions(r: dict) -> str:
    return ";".join(f"{n}={v['px']}/{v['one']}/{v['pct']}/{v['beyond']}" for n, v in r.items())


# ------------------------------------------------------------------------------------------------ the trace: where each checkpoint is
_FUNCS = None


def _funcs():
    global _FUNCS
    if _FUNCS is None:
        rows = []
        for ln in FUNCS_TSV.read_text().splitlines()[1:]:
            f = ln.split("\t")
            for rg in (f[5].split(";") if len(f) > 5 and f[5] else []):
                a, b = rg.split("-")
                rows.append((int(a, 16), int(b, 16), f[1]))
        rows.sort()
        _FUNCS = ([r[0] for r in rows], rows)
    return _FUNCS


def function_at(ra: int) -> str:
    starts, rows = _funcs()
    i = bisect.bisect_right(starts, ra) - 1
    return rows[i][2] if i >= 0 and ra <= rows[i][1] else f"?{ra:x}"


def classify(tr, ordinal: int) -> dict:
    """The frame holding the checkpoint's clock call (0 = the boot, before the first FRAME record) and the context: the clock-call
    sites from that frame's head up to the call. waitTicks and jgld's QPC reads (Palette::update) are shared by the frame and the
    modal loops, so they say nothing; the H1 wait's own read (H1_WAIT) does."""
    frames = [r.f["pos"] for r in tr.records if r.type == TF.FRAME]
    g = bisect.bisect_right(frames, ordinal)
    lo = frames[g - 1] if g > 0 else -1
    seq, rasl = [], []
    for o, ra, _slot, kind, _v in TF.clock_calls(tr):
        if o < lo:
            continue
        if o > ordinal:
            break
        if kind == 1:
            rasl.append(ra)
        if kind != 1:
            seq.append("QPC")
        elif H1_WAIT[0] <= ra <= H1_WAIT[1]:
            seq.append("H1")
        elif ra == TF.FRAME_RA:
            seq.append("HEAD")
        else:
            seq.append(function_at(ra))
    quiet = {"waitTicks", "QPC"}
    ticks = wait_ticks(rasl)
    if g == 0:
        last = [x for x in seq if x not in quiet]
        ctx = "BOOT:" + (last[-1] if last else "-")
    elif seq and seq[-1] == "HEAD":
        ctx = "HEAD"
    elif "H1" in seq:
        k = len(seq) - 1 - seq[::-1].index("H1")
        after = sorted(set(seq[k + 1:]) - quiet - {"H1"})
        n_after = sum(1 for x in seq[k + 1:] if x != "QPC")
        if after:
            ctx = "MODAL_AFTER_H1:" + ",".join(after)
        elif n_after <= H1_WAIT_READS:
            ctx = "H1WAIT"
        elif "QPC" in seq[k + 1:] and ticks == 1:
            # the frame left for the title menu (FrameExit::Menu): showTitleScreen's palette (one jgld QPC read) and the menu's
            # waitTicks(1, 0) loop (tick_menu.inc, ML+0x11a69)
            ctx = "AFTER_H1:title"
        else:
            ctx = "AFTER_H1:waitTicks"
    else:
        other = [x for x in seq if x not in quiet | {"HEAD", "mainLoop"}]
        ctx = "MODAL:" + (other[-1] if other else "waitTicks")
    return {"frame": g, "context": ctx, "wait_ticks": ticks}


def wait_ticks(ras: list[int]) -> int:
    """The n of the waitTicks(n, 0) loop the original sat in at the checkpoint, from the period of its clock reads: a call makes 10 n + 1
    timeGetTime reads (n = 1: 11, cov_b_location back; n = 5: 51, cov_b_location tip). The period is the distance between the last
    two reads at the rarest return address among the last 400 waitTicks reads; -1 when the checkpoint's read is not waitTicks'."""
    tail = [r for r in ras[-400:]]
    if not tail or function_at(tail[-1]) != "waitTicks":
        return -1
    tail = [r for r in tail if function_at(r) == "waitTicks"]
    cnt = {}
    for r in tail:
        cnt[r] = cnt.get(r, 0) + 1
    rare = [r for r, n in cnt.items() if n >= 2]
    if not rare:
        return -1
    r = min(rare, key=lambda x: (cnt[x], x))
    pos = [i for i, x in enumerate(tail) if x == r]
    per = pos[-1] - pos[-2]
    return (per - 1) // 10 if (per - 1) % 10 == 0 else -1


def screen_kind(ctx: str, frame: int, ticks: int, handled: dict) -> str:
    if ctx == "AFTER_H1:title":
        return "title"
    if frame > 0:
        return "frame" if any(m in ctx for m in FULL_SCREEN_MODALS) else "ingame"
    # the boot frame: the title menu's loop waits with waitTicks(1, 0) (tick_menu.inc, ML+0x11a69); the location screen
    # (courseActionMenu, drawn by the run: courseActionMenu.setup handled) waits 5 ticks per pass (cov_b_location site_hover .. tip,
    # cov_l_point location) and is left to through pumpAndRedraw (cov_b_location refused: its read is pumpAndRedraw's); any other boot
    # screen gets the whole frame
    if ticks == 1:
        return "title"
    if (ticks == 5 or ctx == "BOOT:pumpAndRedraw") and handled.get("courseActionMenu.setup", 0) > 0:
        return "location"
    return "frame"


def terrain_rule(ctx: str, frame: int) -> tuple[str, str]:
    """Terrain on screen at the snapshot. mainLoop's frame preamble calls updateTerrainRender unconditionally (FramePreamble,
    tick_frame.inc), which renders the course view or restores it (its blit_screen path); the boot frame holds no course view
    (title menu, location screen, Setup, Session). Checked against the reference frames of every context (notes 2_notes.md: HEAD and
    H1WAIT frames show the course view, the credits cover it, most MODAL frames are popups over it, five are full-screen screens the
    trace cannot tell apart: U-1111)."""
    if frame == 0:
        return "no", "boot frame (no course view yet)"
    if ctx == "AFTER_H1:title":
        return "no", "the frame left for the title menu"
    if any(m in ctx for m in FULL_SCREEN_MODALS):
        return "no", f"full-screen modal {ctx}"
    if ctx in ("HEAD", "H1WAIT"):
        return "yes", f"in-game frame {frame}, {ctx}"
    return "yes", f"in-game frame {frame}, {ctx} over the course view (U-1111)"


# ------------------------------------------------------------------------------------------------ the gfx log
def parse_gfx_log(text: str) -> dict:
    out = {"handled": {}, "unsupported": {}, "snap_unsup": {}, "first": [], "snap_terrain": {}}
    for ln in text.splitlines():
        f = ln.split("\t")
        if f[0] == "handled" and len(f) == 3:
            out["handled"][f[1]] = int(f[2])
        elif f[0] == "unsupported" and len(f) == 3:
            out["unsupported"][f[1]] = int(f[2])
        elif f[0] == "snapshot" and len(f) == 4:
            out["snap_unsup"][int(f[1])] = int(f[3])
        elif f[0] == "first" and len(f) == 3:            # proposed (2_merge.md): the reasons in first-occurrence order
            out["first"].append(f[2])
        elif f[0] == "snapshot_terrain" and len(f) == 3:  # proposed (2_merge.md): a terrain pass is in the snapshot's frame
            out["snap_terrain"][int(f[1])] = int(f[2])
    return out


def reason_name(reason: str) -> str:
    if reason.startswith("no body: "):
        return reason[len("no body: "):]
    return reason.split(": ", 1)[0]


def order_probe(scenario: str, limit: int = PROBE_LIMIT) -> dict:
    """The first TkBase call of every callee name, from a second boot run of the scenario with tick's SG_TICK_TRACE call trace and
    SG_TICK_CALL_LIMIT=limit (a separate process: the trace file is a static of tick.cpp). gfx_drop runs inside TkBase::call right
    after the trace line, so a 'no body: X' reason first occurs at X's first traced call; a body's own reason ('X: why') occurs at
    or after X's first call. Drops that modules hand to gfx_drop directly (saveload.cpp's gx) are not traced. Returns {name: index}."""
    fd, path = tempfile.mkstemp(prefix="framegate_probe_", suffix=".trace")
    os.close(fd)
    env = dict(os.environ, SG_TICK_TRACE=path, SG_TICK_CALL_LIMIT=str(limit))
    first = {}
    try:
        subprocess.run([sys.executable, "-m", "port.test.framegate", "--probe-run", scenario], cwd=str(ROOT), env=env,
                       capture_output=True, text=True, timeout=3600)
        with open(path, "r", errors="replace") as fh:
            for k, ln in enumerate(fh):
                n = ln.split(" ", 1)[0].rstrip("\n")
                if n not in first:
                    first[n] = k
    finally:
        try:
            os.remove(path)
        except OSError:
            pass
    return first


def unsupported_before(glog: dict, k: int, probe: dict | None) -> tuple[list[str], str]:
    """The distinct unsupported reasons before snapshot k, in first-occurrence order, and how the order is known."""
    n = glog["snap_unsup"].get(k, 0)
    if n == 0:
        return [], "none"
    if glog["first"]:
        return glog["first"][:n], "gfx"
    reasons = list(glog["unsupported"])
    if probe:
        ordered = sorted((r for r in reasons if reason_name(r) in probe), key=lambda r: (probe[reason_name(r)], r))
        rest = sorted(r for r in reasons if reason_name(r) not in probe)
        allr = ordered + rest
        exact = all(r.startswith("no body: ") for r in ordered[:n]) and len(ordered) >= n
        if n == len(reasons):
            return allr, "final+probe" if exact else "final"
        return allr[:n], "probe" if exact else "partial"
    if n == len(reasons):
        return sorted(reasons), "final"
    return sorted(reasons)[:n], "partial"


# ------------------------------------------------------------------------------------------------ the run
def scenario_checkpoints(scenario: str) -> list[str]:
    s = json.loads(REFS_JSON.read_text())["scenarios"].get(scenario)
    return list(s["checkpoints"]) if s else []


def all_scenarios() -> list[str]:
    return sorted(json.loads(REFS_JSON.read_text())["scenarios"])


def _bind_live(dll):
    from port.test import terrainlive as TL
    TL.bind(dll)
    dll.sg_ts_live_object.restype, dll.sg_ts_live_object.argtypes = C.c_uint32, [C.c_void_p]
    return TL


def terrain_address(scenario: str) -> int:
    from port.test import terrainlive as TL
    root = TL.TC.CORPUS / ("pin_" + scenario)
    if (root / "index.json").exists():
        return TL.TC.calls(root)[0].ret
    return 0


def run_scenario(scenario: str, checkpoints: list[str] | None = None, lib=None, negctl: tuple[str, int] | None = None,
                 swap: dict | None = None, images: pathlib.Path | None = None, probe: bool = True,
                 probe_limit: int = PROBE_LIMIT, live: bool = True, terrain_va: int | None = None,
                 host_gl: bool | str = False, gl: str = "raster") -> list[dict]:
    """One boot of `scenario` with gfx and LiveTerrain attached; one row per checkpoint. host_gl: the terrain passes drawn by the
    host's own OpenGL (terrainLive.hostGl, port/include/portsim/sim/gl_wgl.h; roadmap S3.15) instead of GlRaster; "try" = ask for
    it and keep GlRaster when the host has none (the fallback), True = refuse to run without it. gl (roadmap S3.20, decision G13;
    used when host_gl is not given): the renderer LiveTerrain::choose_renderer picks, "raster" (GlRaster, the default) or the opt-in
    "auto" (the host's OpenGL when the startup self-check passes, else GlRaster) or "host" / "wgl" (the host's OpenGL without the
    check); rows carry renderer / renderer_reason."""
    lib = lib or L.load()
    backend = L.backend()
    chks = checkpoints or scenario_checkpoints(scenario)
    variant = ",".join(x for x in ([f"negctl {negctl[0]}={negctl[1]}"] if negctl else []) +
                       [f"ref {k}={v}" for k, v in (swap or {}).items()] + (["host-gl"] if host_gl else []) +
                       ([f"gl {gl}"] if gl != "raster" and not host_gl else []))
    base = {"scenario": scenario, "backend": backend, "variant": variant}
    trp = B.TRACES / f"{scenario}.trace"
    if not trp.exists() or not chks:
        why = "no trace" if not trp.exists() else "no checkpoints in scenario_refs.json"
        return [dict(base, checkpoint=c, status="NOT_RUN", detail=f"MISSING: {why}") for c in (chks or ["-"])]
    tr = TF.read(trp)
    tgt = {}
    k = 0
    for ordinal, _ra, _slot, kind, value in TF.clock_calls(tr):
        if kind == 1:
            tgt[k] = (ordinal, value)
            k += 1
    polls = {}
    for r in tr.records:
        if r.type == TF.POLL and r.f.get("checkpoint") and r.f["checkpoint"] not in polls:
            polls[r.f["checkpoint"]] = r.f["pos"]
    summ = json.loads((GX.REFS / scenario / "det_b" / "summary.json").read_text())
    meta, ords = {}, []
    for c in chks:
        st = summ["checkpoints"][c]["status"]
        if st["tgt"] not in tgt or tgt[st["tgt"]][1] != st["T"]:
            meta[c] = {"error": f"the trace's tgt call {st['tgt']} is not the checkpoint's T {st['T']}"}
            continue
        o = tgt[st["tgt"]][0]
        # roadmap S3.16: the clock call the checkpoint's POLL rode on when it is not a tgt call (gfx.checkpoint_ordinal, U-1820)
        if os.environ.get("SG_GFX_CHECKPOINT_AT", "poll") == "poll" and polls.get(c) is not None and polls[c] < o:
            o = polls[c]
        meta[c] = dict(classify(tr, o), ordinal=o)
        ords.append((o, c))
    ords.sort()
    frames_pos = [r.f["pos"] for r in tr.records if r.type == TF.FRAME]
    no_frames = not (B.expected_records(scenario)[1] or [])
    # records through the frame after the last checkpoint's: a checkpoint in a frame's H1 wait is snapshotted at the next cursor
    # read or dispatch past it, which is in the following frame's body (gfx.render stops one frame earlier: course_season hole_open
    # got no snapshot with it)
    max_records = (sum(1 for p in frames_pos if p <= ords[-1][0]) + 2) if ords else 1
    # past the last frame record (the title menu after the session, the quit): the boot runs on to the end of the trace
    to_end = no_frames or max_records > len(frames_pos)
    del tr
    snaps, glog_txt, live_info = {}, "", {"attached": "n/a"}
    renderer = {"renderer": "", "renderer_reason": "no LiveTerrain" if not live else ""}
    use_live = live and lib.has("gfx.attach")
    va = (terrain_address(scenario) if terrain_va is None else terrain_va) if use_live else 0
    from port.test import terrainlive as TLm
    if use_live and getattr(lib, "dll", None) is not None:
        _bind_live(lib.dll)

    def before(st):
        st.call("gfx.attach", [])
        for o, _c in ords:
            st.call("gfx.snapshotAt", [o])
        if use_live:
            if TLm.live_attach(lib, st, va) != 0:
                raise RuntimeError("sg_ts_live_attach failed")
            if host_gl:
                on = st.call("terrainLive.hostGl", [1]).eax == 1
                if not on and host_gl != "try":
                    raise RuntimeError("terrainLive.hostGl: the host has no OpenGL implementation the sink can use")
                renderer.update(renderer="wgl" if on else "raster", renderer_reason="--host-gl" + ("" if on else " (no host OpenGL)"))
            else:
                st.call("terrainLive.renderer", [GL_MODES[gl]])
                t = (st.files_read("$wgl/renderer.txt") or b"").decode(errors="replace")
                kv = dict(ln.split(" ", 1) for ln in t.splitlines() if " " in ln)
                renderer.update(renderer=kv.get("backend", "?"), renderer_reason=f"{kv.get('mode', gl)}: {kv.get('reason', '')}"
                                + (f" [check {kv.get('check')} {kv.get('ms')} ms]" if kv.get("check") not in (None, "none") else ""))

    def after(st, _res):
        nonlocal glog_txt
        for i, (_o, c) in enumerate(ords):
            raw = st.files_read(f"$gfx/snapshot_{i}.555")
            snaps[c] = None if raw is None or len(raw) != W * H * 2 else \
                GX.imgcmp.rgb555_to_rgb(np.frombuffer(raw, "<u2").reshape(H, W))
        st.call("gfx.log", [])
        t = st.files_read("$gfx/log.txt")
        glog_txt = t.decode() if t else ""
        st.call("gfx.detach", [])
        if use_live:
            names = [ln.split()[3] for ln in TLm.live_log(lib, st).splitlines() if len(ln.split()) > 3]
            live_info.update(attached=f"0x{TLm.live_object(lib, st):x}", calls=len(names),
                             renders=sum(1 for x in names if x in ("render", "localRender", "pathUpdateRender")),
                             init=int("initSystem" in names and "initTerrain" in names))
            TLm.live_detach(lib, st)

    prev = lib.negctl(negctl[0], negctl[1]) if negctl else None
    t0 = time.time()
    try:
        bres = B.run(scenario, lib, max_records=max_records, before=before, after=after, to_end=to_end)
    finally:
        if negctl:
            lib.negctl(negctl[0], prev or 0)
    secs = round(time.time() - t0, 1)
    glog = parse_gfx_log(glog_txt)
    # the order of the reasons is needed for any snapshot with one before it (its first unsupported call), unless the backend
    # gave its own first-occurrence list
    need_probe = probe and not glog["first"] and any(glog["snap_unsup"].get(i, 0) > 0 for i in range(len(ords)))
    pr = order_probe(scenario, probe_limit) if need_probe else None
    live_txt = live_info["attached"] if live_info["attached"] == "n/a" else \
        f"{live_info['attached']} calls={live_info['calls']} renders={live_info['renders']} init={live_info['init']}"
    div = (bres.first_divergence or {}).get("record")
    if div is None and bres.status == "FAIL" and bres.records < bres.expected:
        # the run stopped without a divergence (sg_run_boot returned an error, e.g. a missing name, after `records` equal
        # records): the checkpoints before that record are still judged (S2.8, U-1270)
        div = bres.records
    rows = []
    for i, (o, c) in enumerate(ords):
        m = meta[c]
        row = dict(base, checkpoint=c, context=m["context"], frame=m["frame"], wait_ticks=m["wait_ticks"], boot_status=bres.status,
                   boot_records=f"{bres.records}/{bres.expected}", liveterrain=live_txt, seconds=secs, **renderer)
        kind = screen_kind(m["context"], m["frame"], m["wait_ticks"], glog["handled"])
        row["kind"] = kind
        row["terrain"], row["terrain_rule"] = terrain_rule(m["context"], m["frame"])
        if i in glog["snap_terrain"]:
            row["terrain_rule"] += f"; gfx snapshot_terrain={glog['snap_terrain'][i]}"
            row["terrain_drawn"] = glog["snap_terrain"][i]
        ub, order = unsupported_before(glog, i, pr)
        drawing = [r for r in ub if reason_name(r) not in NOT_DRAWING]
        row.update(unsup_before=glog["snap_unsup"].get(i, 0), unsupported_before="|".join(ub),
                   first_unsupported=ub[0] if ub else "", first_unsupported_drawing=drawing[0] if drawing else "",
                   unsup_order=order)
        refname = (swap or {}).get(c, f"{scenario}/{c}")
        row["reference"] = refname
        img = snaps.get(c)
        bad_boot = bres.status not in ("PASS", "KNOWN", "NO_FRAMES") and (div is None or m["frame"] >= div)
        if img is None:
            row.update(status="NOT_RUN", detail=f"no snapshot (boot {bres.status}: {bres.detail[:120]})")
            rows.append(row)
            continue
        rs, rc = refname.split("/", 1)
        ref = GX.reference(rs, rc)
        rn = region_numbers(img, ref, kind)
        tot = sum(v["px"] for v in rn.values())
        one = sum(v["one"] for v in rn.values())
        row.update(total_px=tot, one_level_px=one, one_level_pct=round(100.0 * one / (W * H), 3), beyond_px=tot - one,
                   regions=fmt_regions(rn))
        if bad_boot:
            row.update(status="NOT_RUN", detail=f"boot {bres.status} at record {div}: {bres.detail[:120]}")
        elif tot == 0:
            row.update(status="PASS", detail="")
        elif drawing:
            row.update(status="UNSUPPORTED", detail=drawing[0])
        else:
            row.update(status="FAIL", detail=",".join(f"{n}={v['px']}" for n, v in rn.items() if v["px"]))
        if images:
            from PIL import Image
            images.mkdir(parents=True, exist_ok=True)
            Image.fromarray(img).save(images / f"{scenario}.{c}.port.png")
            Image.fromarray(((img != ref).any(axis=2) * 255).astype(np.uint8)).save(images / f"{scenario}.{c}.diff.png")
        rows.append(row)
    for c in chks:
        if "error" in meta.get(c, {}):
            rows.append(dict(base, checkpoint=c, status="NOT_RUN", detail=meta[c]["error"]))
    order_ix = {c: i for i, c in enumerate(chks)}
    rows.sort(key=lambda r: order_ix.get(r["checkpoint"], 1 << 30))
    return rows


# ------------------------------------------------------------------------------------------------ CLI
def effective_jobs(n: int) -> int:
    return max(1, min(n, MAX_JOBS))


def _worker_args(a) -> list[str]:
    out = []
    if a.checkpoints:
        out += ["--checkpoints", a.checkpoints]
    if a.negctl:
        out += ["--negctl", a.negctl]
    for s in a.swap_ref or []:
        out += ["--swap-ref", s]
    if a.images:
        out += ["--images", str(a.images)]
    if a.no_probe:
        out.append("--no-probe")
    if a.no_live:
        out.append("--no-live")
    if a.host_gl:
        out.append("--host-gl")
    if a.gl != "raster":
        out += ["--gl", a.gl]
    out += ["--probe-limit", str(a.probe_limit)]
    return out


def _signature(a) -> str:
    return json.dumps({"backend": L.backend(), "args": _worker_args(a)}, sort_keys=True)


def write_tsv(path: pathlib.Path, rows: list[dict]):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(COLUMNS) + "\n")
        for r in rows:
            f.write("\t".join(str(r.get(k, "")).replace("\t", " ").replace("\n", " ") for k in COLUMNS) + "\n")


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scenarios", nargs="*")
    ap.add_argument("--all", action="store_true", help="every scenario of re/analysis/data/scenario_refs.json")
    ap.add_argument("--checkpoints", help="comma list (one scenario)")
    ap.add_argument("--jobs", type=int, default=1, help=f"scenario processes at once (at most {MAX_JOBS})")
    ap.add_argument("--out", type=pathlib.Path, help="the TSV (per-scenario results are kept in <out>.d/ for --resume)")
    ap.add_argument("--resume", action="store_true", help="reuse <out>.d/<scenario>.json made with the same options")
    ap.add_argument("--negctl", help="name=mode, e.g. gfx.attach=1")
    ap.add_argument("--swap-ref", action="append", help="<checkpoint>=<scenario>/<checkpoint>: compare against another reference")
    ap.add_argument("--images", type=pathlib.Path)
    ap.add_argument("--no-probe", action="store_true")
    ap.add_argument("--no-live", action="store_true", help="LiveTerrain not attached (development)")
    ap.add_argument("--probe-limit", type=int, default=PROBE_LIMIT)
    ap.add_argument("--host-gl", action="store_true", help="terrain drawn by the host's own OpenGL (roadmap S3.15)")
    ap.add_argument("--gl", choices=sorted(GL_MODES), default="raster",
                    help="the terrain renderer (roadmap S3.20): raster (the default) = GlRaster; opt-in auto = the host's OpenGL when "
                         "its self-check passes, else GlRaster; host (= wgl) = the host's OpenGL without the check")
    ap.add_argument("--worker", help=argparse.SUPPRESS)
    ap.add_argument("--probe-run", help=argparse.SUPPRESS)
    a = ap.parse_args(argv)
    if a.probe_run:     # the order probe's run (SG_TICK_TRACE / SG_TICK_CALL_LIMIT set by order_probe)
        try:
            run_scenario(a.probe_run, probe=False)
        except Exception as e:  # noqa: BLE001 - the call limit ends the run; the trace file is what counts
            print(f"probe run ended: {e!r}")
        return 0
    negctl = None
    if a.negctl:
        n, m = a.negctl.rsplit("=", 1)
        negctl = (n, int(m))
    swap = dict(s.split("=", 1) for s in (a.swap_ref or []))
    chks = a.checkpoints.split(",") if a.checkpoints else None
    if a.worker:
        rows = run_scenario(a.worker, chks, negctl=negctl, swap=swap, images=a.images, probe=not a.no_probe,
                            probe_limit=a.probe_limit, live=not a.no_live, host_gl=a.host_gl, gl=a.gl)
        print(json.dumps(rows))
        return 0
    names = all_scenarios() if a.all else a.scenarios
    if not names:
        ap.error("no scenario")
    jobs = effective_jobs(a.jobs)
    state = pathlib.Path(str(a.out) + ".d") if a.out else None
    sig = _signature(a)
    results, todo = {}, []
    for s in names:
        p = state / f"{s}.json" if state else None
        if a.resume and p and p.exists():
            d = json.loads(p.read_text())
            if d.get("signature") == sig:
                results[s] = d["rows"]
                print(f"[resume] {s}: {len(d['rows'])} rows", flush=True)
                continue
        todo.append(s)

    def one(s):
        cmd = [sys.executable, "-m", "port.test.framegate", "--worker", s] + _worker_args(a)
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ROOT))
        lines = r.stdout.strip().splitlines()
        if r.returncode == 0 and lines and lines[-1].startswith("["):
            return json.loads(lines[-1])
        return [{"scenario": s, "checkpoint": c, "status": "NOT_RUN", "detail": f"ERROR: {(r.stderr or r.stdout)[-300:]}"}
                for c in (chks or scenario_checkpoints(s) or ["-"])]

    n_done = 0
    with cf.ThreadPoolExecutor(jobs) as ex:
        futs = {ex.submit(one, s): s for s in todo}
        for f in cf.as_completed(futs):
            s = futs[f]
            rows = f.result()
            results[s] = rows
            n_done += 1
            if state:
                state.mkdir(parents=True, exist_ok=True)
                (state / f"{s}.json").write_text(json.dumps({"signature": sig, "rows": rows}))
            counts = {}
            for r in rows:
                counts[r["status"]] = counts.get(r["status"], 0) + 1
            print(f"[{n_done}/{len(todo)}] {s} {' '.join(f'{k}={v}' for k, v in sorted(counts.items()))} "
                  f"({rows[0].get('seconds', '-') if rows else '-'} s)", flush=True)
    allrows = [r for s in names for r in results.get(s, [])]
    for r in allrows:
        if a.out is None:
            print("\t".join(str(r.get(k, "")) for k in ("scenario", "checkpoint", "status", "detail", "kind", "context", "terrain",
                                                          "total_px", "unsup_before", "first_unsupported", "boot_status")))
    if a.out:
        write_tsv(a.out, allrows)
        counts = {}
        for r in allrows:
            counts[r["status"]] = counts.get(r["status"], 0) + 1
        print(json.dumps({"rows": len(allrows), "counts": counts}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
