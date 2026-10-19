"""Phase 3.3 bridge module (worldToScreen, playSoundAt, queueMessage, pointsPopup): no corpus vectors exist for these four
(they were not in the phase 2.5 selection), so the oracle is the C3 path-1 A/B runs of the original
(log/diff/<addr>_<name>*.path1.csv: per vector the arguments, the ORIGINAL's return value and an FNV-1a hash of the
compared state regions after the call). Each test rebuilds the run's fixture in a zero GameState exactly as the fixture
script describes it (re/frida/js/diff_fixtures.js, fixtures.d/c3p.js, fixtures.d/c3dy.js; the registry entries are in
re/frida/hooks_registry.py and registry.d/), calls the port, hashes the same regions the same way (diff_hook.js: the hex of
each region joined by '|') and compares return and hash. Rows whose result depends on game state the fixture did not set
are listed with the reason and skipped (counted in the summary). Notes: log/p3/p33_notes.txt."""
from __future__ import annotations

import csv
import pathlib
import struct
import sys

import pytest

from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = pytest.mark.port
DIFF = ROOT / "log" / "diff"
need_runs = pytest.mark.skipif(not (DIFF / "0042fb90_worldToScreen.path1.csv").exists(), reason="C3 path-1 runs not present")
DATA = VL.DATA_VA


def fnv(s: str) -> str:
    h = 0x811C9DC5
    for ch in s:
        h ^= ord(ch)
        h = (h * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


def hexs(*bufs: bytes) -> str:
    return "|".join(b.hex() for b in bufs)


def rows(name: str):
    """The vector rows of a path-1 CSV (the trailing state_regions / meta / VERDICT rows are dropped)."""
    with (DIFF / name).open(newline="") as f:
        return [r for r in csv.DictReader(f) if r.get("match") in ("True", "False") and r.get("state_original")]


class G:
    """GameState writes by original data address (tooling: the fixtures are written in those terms)."""

    def __init__(self, st):
        self.st = st

    def w(self, va, fmt, *v):
        self.st.write(va - DATA, struct.pack("<" + fmt, *v))

    def wb(self, va, data: bytes):
        self.st.write(va - DATA, data)

    def r(self, va, n):
        return self.st.read(va - DATA, n)


# ------------------------------------------------------------------------------------------- worldToScreen (c3p)
def _tile_types(g):
    g.wb(0x005722E8, bytes(((x * 7 + y * 3) & 0x1F) for x in range(50) for y in range(50)))


def _w2s_view(g, rot, W, dbl):
    """c3p.js _c3p_view(rot, W, dbl, 'w2s') over terrain_slopes / golf_tables / tile_types_pattern (the parts the
    projection reads: tile types, type flags, the corner cache, the level grid, the camera globals)."""
    _tile_types(g)
    cache = bytearray(2500 * 8)
    for x in range(50):
        for y in range(50):
            for k in range(8):
                cache[(x * 50 + y) * 8 + k] = (x * 3 + y * 5 + k * 3) % 7 + 1
    for t in range(32):
        g.w(0x0057837C + t * 0x30, "I", [8, 2, 4, 0, 0, 1][t % 6])
    for x in range(50):
        for y in range(50):
            p, b, o = (x * 3 + y) % 4, 1 + x % 5, (x * 50 + y) * 8
            if p == 0:
                cache[o + 1] = cache[o + 3] = cache[o + 5] = cache[o + 7] = b
            if p == 1:
                cache[o + 1], cache[o + 3], cache[o + 5], cache[o + 7] = b + 1, b + 2, b, b
            if p == 2:
                cache[o + 1], cache[o + 3], cache[o + 5], cache[o + 7] = b, b + 1, b, b
    g.wb(0x0051B770, bytes(cache))
    g.wb(0x00543018, bytes((((i * 7) % 13 - 4) & 0xFF) for i in range(2500)))
    g.w(0x004C2BA0, "ii", 25, 25)
    g.w(0x004C2844, "i", 4)
    g.w(0x004C2840, "i", 16)
    g.w(0x00822C8C, "ii", W, 600)
    g.w(0x005685F4, "i", rot)
    g.w(0x005A9CC0, "i", dbl)
    g.w(0x004C2E00, "i", 5)


W2S_KEYS = {"": (0, 800, 0), "_r2": (2, 800, 0), "_r4": (4, 800, 0), "_r6": (6, 800, 0), "_w400": (0, 0x400, 0),
            "_w500": (0, 0x500, 0), "_dbl": (0, 800, 1), "_r1": (1, 800, 0)}
R1_PRE = {"in": (100, 100), "xl": (-500, 100), "xh": (2000, 100), "yl": (100, -500), "yh": (100, 2000)}


def _w2s_uses_live_state(wx, wy, sx, sy, rot, W, dbl):
    """A projection that passes the first screen test reads the tile's flags; flags 2 (types t % 6 == 1) go through
    cornerRange / heightBlend, which read terrain grids the fixture leaves as the running game had them."""
    t = ((wx >> 10) * 7 + (wy >> 10) * 3) & 0x1F if 0 <= wx >> 10 < 50 and 0 <= wy >> 10 < 50 else None
    return t is not None and t % 6 == 1


def _run_w2s(lib, key, mode=0):
    rot, W, dbl = W2S_KEYS[key]
    lib.negctl("worldToScreen", mode)
    out = {"match": 0, "mismatch": [], "live": 0}
    for row in rows(f"0042fb90_worldToScreen{key}.path1.csv"):
        wx, wy, a_sx, a_sy, m = row["vector"].split()
        wx, wy, m = int(wx), int(wy), int(m)
        with lib.state() as st:
            st.set_data_base(DATA)
            g = G(st)
            _w2s_view(g, rot, W, dbl)
            pre = R1_PRE[a_sx.split("_", 1)[1]] if rot == 1 else (0, 0)
            bsx = st.map(0x40000010, bytearray(struct.pack("<i", pre[0])))
            bsy = st.map(0x40001020, bytearray(struct.pack("<i", pre[1])))
            try:
                r = st.call("worldToScreen", [wx & 0xFFFFFFFF, wy & 0xFFFFFFFF, 0x40000010, 0x40001020, m]).eax
            except L.SgError as e:
                if mode == 0 and _w2s_uses_live_state(wx, wy, 0, 0, rot, W, dbl):
                    out["live"] += 1
                else:
                    out["mismatch"].append((row["vector"], f"error {e}"))
                continue
            if rot == 1:   # the r1 key's state is all ten pre-filled cells; only this vector's pair can change
                cells = {k: struct.pack("<ii", *v) for k, v in R1_PRE.items()}
                cells[a_sx.split("_", 1)[1]] = bytes(bsx) + bytes(bsy)
                h = fnv(hexs(*[cells[k][:4] for k in R1_PRE], *[cells[k][4:] for k in R1_PRE]))
            else:
                h = fnv(hexs(bytes(bsx), bytes(bsy)))
        good = str(struct.unpack("<i", struct.pack("<I", r))[0]) == row["original"] and h == row["state_original"]
        if good:
            out["match"] += 1
        elif mode == 0 and _w2s_uses_live_state(wx, wy, 0, 0, rot, W, dbl):
            out["live"] += 1
        else:
            out["mismatch"].append((row["vector"], f"ret {r} vs {row['original']}, state {h} vs {row['state_original']}"))
    lib.negctl_reset()
    return out


@need_runs
@pytest.mark.parametrize("key", list(W2S_KEYS))
def test_world_to_screen_replays_c3(portlib, key):
    if "cornerHeights" not in portlib.functions():
        pytest.skip("cornerHeights (3.1a) not registered")
    out = _run_w2s(portlib, key)
    print(f"worldToScreen{key}: {out['match']} exact, {out['live']} on flag-2 tiles (live terrain grids), "
          f"{len(out['mismatch'])} mismatches")
    assert not out["mismatch"], out["mismatch"][:5]
    assert out["match"] >= 10


# negative controls: each mode must turn at least one exactly reproduced row into a mismatch
W2S_NEGCTL = [(1, "_r2", "rotation 2 y offset + 5 Z"), (2, "", "second screen test without margin"),
              (3, "", "TTA_LEVEL_MAX heights without - 3")]


@need_runs
@pytest.mark.parametrize("mode,key,what", W2S_NEGCTL)
def test_world_to_screen_negctl(portlib, mode, key, what):
    if "cornerHeights" not in portlib.functions():
        pytest.skip("cornerHeights (3.1a) not registered")
    out = _run_w2s(portlib, key, mode)
    assert out["mismatch"], f"worldToScreen mode {mode} ({what}) reproduced every row"


# ------------------------------------------------------------------------------------------- queueMessage / pointsPopup
MSG_REGIONS = [(0x0053BBA8, 4), (0x0056C770, 32), (0x0056C794, 32), (0x0056A794, 32), (0x0056A924, 32), (0x0056C570, 512)]
POP_REGIONS = [(0x0059ABB0, 4), (0x00542FD8, 32), (0x00542FF8, 32), (0x00542DD8, 32), (0x00542F00, 32),
               (0x00575CA0, 4 * 0x208)]


def _msg_fixture(g, n):
    """diff_fixtures.js _msgs(n): the text 'ab-msg', ring words a = i % 3, b = i % 2, ttl = 0x30 - i, write index n. The
    other bytes of the regions are the main menu's (zero there: the replay reproduces every row with zeros)."""
    g.wb(0x0051A068, b"ab-msg\0")
    for i in range(8):
        g.w(0x0056C770 + 4 * i, "i", i % 3)
        g.w(0x0056C794 + 4 * i, "i", i % 2)
        g.w(0x0056A924 + 4 * i, "i", 0x30 - i)
    g.w(0x0053BBA8, "i", n)


def _pop_fixture(g, flag, n):
    """diff_fixtures.js _pops(flag, n): g_flags bit 0x1000000, write index n, money of hole records 1..4 = 1000 + who."""
    g.w(0x0059E7B8, "I", 0x1000000 if flag else 0)
    g.w(0x0059ABB0, "i", n)
    for w in range(4):
        g.w(0x00575CA0 + w * 0x208, "i", 1000 + w)


def _run_ring(lib, fn, csvname, fixture, regions, mode=0):
    lib.negctl(fn, mode)
    good = bad = 0
    for row in rows(csvname):
        args = [int(v) & 0xFFFFFFFF for v in row["vector"].split()]
        with lib.state() as st:
            st.set_data_base(DATA)
            g = G(st)
            fixture(g)
            st.call(fn, args)
            h = fnv(hexs(*[g.r(va, n) for va, n in regions]))
        good, bad = (good + 1, bad) if h == row["state_original"] else (good, bad + 1)
    lib.negctl_reset()
    return good, bad


RINGS = [
    ("queueMessage", "0040c720_queueMessage.path1.csv", lambda g: _msg_fixture(g, 7), MSG_REGIONS),
    ("queueMessage", "0040c720_queueMessage_n3.path1.csv", lambda g: _msg_fixture(g, 3), MSG_REGIONS),
    ("pointsPopup", "0040c890_pointsPopup.path1.csv", lambda g: _pop_fixture(g, False, 7), POP_REGIONS),
    ("pointsPopup", "0040c890_pointsPopup_n2.path1.csv", lambda g: _pop_fixture(g, False, 2), POP_REGIONS),
    ("pointsPopup", "0040c890_pointsPopup_off.path1.csv", lambda g: _pop_fixture(g, True, 4), POP_REGIONS),
]


@need_runs
@pytest.mark.parametrize("fn,csvname,fixture,regions", RINGS, ids=[r[1] for r in RINGS])
def test_rings_replay_c3(portlib, fn, csvname, fixture, regions):
    good, bad = _run_ring(portlib, fn, csvname, fixture, regions)
    print(f"{csvname}: {good} exact, {bad} mismatches")
    assert bad == 0 and good >= 4


@need_runs
@pytest.mark.parametrize("fn,mode,idx", [("queueMessage", 1, 0), ("queueMessage", 2, 1), ("pointsPopup", 1, 2),
                                         ("pointsPopup", 2, 4)])
def test_rings_negctl(portlib, fn, mode, idx):
    _, csvname, fixture, regions = RINGS[idx]
    good, bad = _run_ring(portlib, fn, csvname, fixture, regions, mode)
    assert bad > 0, f"{fn} mode {mode} reproduced every row of {csvname}"


# ------------------------------------------------------------------------------------------- playSoundAt (c3dy)
# c3dy.js AT rows: sound, wx, wy, field34, the worldToScreen stand-in's answer (r, sx, sy; write False leaves the caller's
# slots), globals (unk_005a9cbc, unk_005a9cd8, unk_005a9cdc, g_zoomScale, g_rng seed)
AT = [
    (5, 1000, 2000, 5, (0, 100, 100), (0, 0, 0, 2, 0x1111)), (6, 1001, 2001, 7, (0, 100, 100), (3, 1, 9, 5, 0x2222)),
    (7, 1100, 2100, -1, (0, -50, 700), (0, 0, 0, 2, 0x3333)), (8, 1200, 2200, -1, (0, 900, -9), (0, 1, 0, 4, 0x4444)),
    (9, 1300, 2300, -1, (1, 400, 300), (0, 0, 0, 5, 0x5555)), (10, 1400, 2400, -1, (1, 123, 299), (7, 1, 35, 0, 0x6666)),
    (11, 1500, 2500, 9, (1, 400, 100), (0, 0, 0, 3, 0x7777)), (12, 1600, 2600, 9, (1, 799, 500), (0, 1, 36, 1, 0x8888)),
    (13, 1700, 2700, 9, (1, 0, 0), (0, 1, 1000, 4, 0x9999)), (14, 1800, 2800, 3, (1, 300, -1), (0, 0, 0, 2, 0xAAAA)),
    (15, 1900, 2900, 3, (1, 300, 501), (0, 0, 0, 2, 0xBBBB)), (16, 2000, 3000, 3, (1, -1, 200), (0, 0, 0, 2, 0xCCCC)),
    (17, 2100, 3100, 3, (1, 800, 200), (0, 0, 0, 2, 0xDDDD)), (18, 2200, 3200, 4, (1, 300, 250), (1, 0, 0, 2, 0xEEEE)),
    (19, 2300, 3300, 4, (1, 700, 250), (1, 0, 0, 2, 0xFFFF)), (20, 2400, 3400, 4, (1, 400, 450), (2, 0, 0, 6, 0x1357)),
    (21, 2500, 3500, 4, (1, 400, 451), (2, 0, 0, 6, 0x2468)), (22, 2600, 3600, 5, (1, 250, 300), (5, 1, -1, 2, 0x1)),
    (23, 2700, 3700, 5, (1, 250, 300), (0, 1, -40, 2, 0x1)), (24, 300, 200, 2, (1, None, None), (0, 0, 0, 2, 0x7AB)),
    (25, 1000, -5, -1, (0, None, None), (0, 1, 18, 2, 0x7AB)), (26, 600, 700, 8, (1, 77, 33), (0, 0, 0, -1, 0x3A3A3A3A)),
    (27, 610, 710, 8, (1, 640, 111), (0, 1, 5, 2, 0x4B4B)), (28, 3000, 4000, 4, (1, 300, 200), (1, 0, 0, 2, 0xABC)),
    (29, 3100, 4100, 4, (1, 200, 300), (1, 1, 7, 2, 0xDEF)), (30, 3200, 4200, 4, (1, 599, 250), (1, 0, 0, 4, 0x123)),
    (31, 3300, 4300, 4, (1, 600, 250), (1, 0, 0, 4, 0x456)),
]
W2S_VA, PS_VA = 0x0042FB90, 0x004481B0       # tooling: the call-log tags the C3 stand-ins wrote (the original's addresses)


def _run_at(lib, mode=0):
    """Replay of 0040c500_playSoundAt.path1.csv through the test-only entry bridge.playSoundAtScripted. The state hash
    covers the call log ($clog: cursor, per-stub counts, entries {tag, count, args...} from +0x50, the stand-ins'
    out-pointer arguments logged as 0xfeedf00d plus the pointed-to word) and the four words."""
    lib.negctl("playSoundAt", mode)
    good, bad = 0, []
    for i, row in enumerate(rows("0040c500_playSoundAt.path1.csv")):
        snd, wx, wy, f34, (wr, wsx, wsy), (dbl, flag, ctr, zoom, seed) = AT[i]
        with lib.state() as st:
            st.set_data_base(DATA)
            st.fill_from_exe(whole_image=True)       # the variation table g_soundVariationCycle is initialised data
            g = G(st)
            g.w(0x005A9CBC, "i", dbl)
            g.w(0x005A9CD8, "ii", flag, ctr)
            g.w(0x004C2844, "i", zoom)
            g.w(0x00822D9C, "I", seed)
            blk = bytearray(0x60)
            struct.pack_into("<iiiii", blk, 0, wr, 0 if wsx is None else 1, wsx or 0, wsy or 0,
                             (0x5050 + (snd & 0xFF)) & 0xFFFFFFFF)
            b = st.map(0x40000010, blk)
            ret = st.call("bridge.playSoundAtScripted", [snd & 0xFFFFFFFF, wx & 0xFFFFFFFF, wy & 0xFFFFFFFF,
                                                         f34 & 0xFFFFFFFF, 0x40000010]).eax
            out = bytes(b)
            words = g.r(0x005A9CBC, 4), g.r(0x005A9CD8, 4), g.r(0x005A9CDC, 4), g.r(0x00822D9C, 4)
        n_w, x, y, m, sxi, syi = struct.unpack_from("<Iiiiii", out, 0x20)
        n_p, *pargs = struct.unpack_from("<Iiiiii", out, 0x40)
        clog = bytearray(0x400)
        cur = 0x50
        struct.pack_into("<II", clog, 4, n_w, n_p)
        if n_w:
            struct.pack_into("<II7I", clog, cur, W2S_VA, 7, *[v & 0xFFFFFFFF for v in (x, y, 0xFEEDF00D, 0xFEEDF00D, m, sxi, syi)])
            cur += 8 + 28
        if n_p:
            struct.pack_into("<II5I", clog, cur, PS_VA, 5, *[v & 0xFFFFFFFF for v in pargs])
            cur += 8 + 20
        struct.pack_into("<I", clog, 0, cur)
        h = fnv(hexs(bytes(clog), *words))
        r = str(struct.unpack("<i", struct.pack("<I", ret))[0])
        if h == row["state_original"] and r == row["original"]:
            good += 1
        else:
            bad.append((i, f"ret {r} vs {row['original']}, state {h} vs {row['state_original']}"))
    lib.negctl_reset()
    return good, bad


@need_runs
@pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
def test_play_sound_at_replays_c3(portlib):
    portlib.exe_load()
    good, bad = _run_at(portlib)
    print(f"playSoundAt: {good} exact, {len(bad)} mismatches")
    assert not bad, bad[:5]
    assert good == len(rows("0040c500_playSoundAt.path1.csv"))


@need_runs
@pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
@pytest.mark.parametrize("mode,what", [(1, "clamp arm keeps field34"), (2, "pitch 0x12d - draw"), (3, "counter not advanced")])
def test_play_sound_at_negctl(portlib, mode, what):
    portlib.exe_load()
    _, bad = _run_at(portlib, mode)
    assert bad, f"playSoundAt mode {mode} ({what}) reproduced every row"


def test_play_sound_at_draws_the_rng_only_on_screen(portlib):
    """The camera dependence of the simulation RNG: the same call draws g_rng when worldToScreen puts the point on screen
    and does not when the camera looks elsewhere (zoom 4, 800 x 600, rotation 0, tile type 0 flagged TTA_HEIGHT_3)."""
    off = {n: o for n, o, s in portlib.objects()}
    for cam, drawn in ((0, True), (40, False)):
        with portlib.state() as st:
            st.set_data_base(DATA)
            st.write(off["g_zoomScale"], struct.pack("<i", 4))
            st.write(off["g_screenWidth"], struct.pack("<ii", 800, 600))
            st.write(off["g_tileTypeAttrs"] + 0x2c, struct.pack("<I", 8))
            st.write(off["gCameraTileX"], struct.pack("<ii", cam, cam))
            st.write(off["g_rng"], struct.pack("<I", 0x1234))
            st.call("playSoundAt", [0x8f, 0x600, 0x600, 0])
            assert (struct.unpack("<I", st.read(off["g_rng"], 4))[0] != 0x1234) == drawn
