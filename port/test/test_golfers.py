"""Phase 3.3 + 3.6 golfers module: negative controls of every function, and state tests for the branches the vectors
miss (thin / one-sided / none coverage, re/port/vectors_coverage.tsv). Notes: log/p3/p33_notes.txt."""
from __future__ import annotations

import pathlib
import struct
import sys

import pytest

from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = pytest.mark.port
need_corpus = pytest.mark.skipif(not VL.VEC_DIR.exists(), reason="vector corpus not present (port/corpus/vectors, local)")

# (function, mode, what): wrong bodies the vectors must see (RED)
NEGCTL_RED = [
    ("appendPairTrackPoint", 1, "point 0 takes the ball position"),
    ("addGolferPair", 1, "partner snapshot of the golfer itself"),
    ("resetGolfer", 2, "dedication tile_y freed"),
    ("golferAge", 1, "bit 2 adds 40"),
    ("typeBit7Clear", 1, "bit 6"),
    ("appendEndearment", 1, "phrases 1 / 2 swapped"),
    ("appendWalkerName", 1, "table strings swapped"), ("appendWalkerName", 2, "flag bit 2"),
    ("spawnObject", 1, "row / column swapped"), ("spawnObject", 2, "sprite base 0x14d"),
    ("spawnWalker", 1, "direction without -2"), ("spawnWalker", 2, "start offset 0x400"),
    ("addHabitatSlot", 1, "state -10"), ("addHabitatSlot", 2, "link 0"),
    ("tickAnimals", 1, "last record skipped"),
    ("placeHabitat", 1, "flag mask 0xdf"),
    ("initGolferState", 1, "one golfer per open hole"),
    ("addGolferToTable", 1, "speed / 4"), ("addGolferToTable", 2, "forced event bit kept"),
    ("loadGolfPro", 1, "flag 0x100000"),
]
# wrong bodies of a function without vectors of its own, read RED through a caller's vectors: (function, mode, caller, what)
NEGCTL_RED_VIA = [
    ("placeAnimal", 2, "tickAnimals", "the wait is not counted down"),
]
# wrong bodies whose function needs a peer that is not registered yet (skipped until it is)
NEGCTL_RED_PEER = [
    ("updateWalkers", 2, "pathStep", "leaving walkers at speed 4"),
    ("updateWalkers", 3, "pathStep", "walking sequence wraps after 9"),
]
# wrong bodies the vectors CANNOT see; each is caught by the state test named in the third column
NEGCTL_BLIND = [
    ("appendPairTrackPoint", 2, "count cap 32: no vector starts at count 31 (test_track_point_cap)"),
    ("addGolferPair", 2, "count not reset: every free slot's count is already 0 (test_add_golfer_pair_slots)"),
    ("resetGolfer", 1, "hole byte kept: every vector's hole byte is already 0 (test_reset_golfer_clears_hole)"),
    ("tickAnimals:placeAnimal", 1, "on-screen frame wrap: the tick vectors are all off screen (test_place_animal_on_screen)"),
    ("updateWalkers", 1, "sale cash +3: no vector reaches the sale arm (test_update_walkers_sale)"),
]
# no vectors at all: the state tests below run every mode (test_reset_golfer_state_*, test_compose_golfer_sprite_*)
NO_VECTORS = ["resetGolferState", "composeGolferSprite"]
FUNCTIONS = sorted({f.split(":")[-1] for f, _, _ in NEGCTL_RED + NEGCTL_BLIND} | {f for f, _, _, _ in NEGCTL_RED_VIA}
                   | {f for f, _, _, _ in NEGCTL_RED_PEER} | set(NO_VECTORS))

GOLFER, TRACK = 0x100, 0x388


def off(lib, name):
    return next(o for n, o, s in lib.objects() if n == name)


@need_corpus
@pytest.mark.parametrize("function,mode,what", NEGCTL_RED)
def test_negctl_reads_red(portlib, vrunner, function, mode, what):
    assert portlib.negctl(function, mode) == 0
    r = vrunner.run_function(function)
    assert r.verdict == "RED" and r.failed > 0, f"{function} mode {mode} ({what}) read {r.summary()}"
    portlib.negctl_reset()
    assert vrunner.run_function(function).verdict == "GREEN"


@need_corpus
@pytest.mark.parametrize("function,mode,caller,what", NEGCTL_RED_VIA)
def test_negctl_reads_red_via_caller(portlib, vrunner, function, mode, caller, what):
    assert portlib.negctl(function, mode) == 0
    r = vrunner.run_function(caller)
    assert r.verdict == "RED" and r.failed > 0, f"{function} mode {mode} ({what}) read {r.summary()}"
    portlib.negctl_reset()
    assert vrunner.run_function(caller).verdict == "GREEN"


@need_corpus
@pytest.mark.parametrize("function,mode,peer,what", NEGCTL_RED_PEER)
def test_negctl_reads_red_with_peer(portlib, vrunner, function, mode, peer, what):
    needs(portlib, peer)
    assert portlib.negctl(function, mode) == 0
    r = vrunner.run_function(function)
    assert r.verdict == "RED" and r.failed > 0, f"{function} mode {mode} ({what}) read {r.summary()}"
    portlib.negctl_reset()
    assert vrunner.run_function(function).verdict == "GREEN"


@need_corpus
@pytest.mark.parametrize("function,mode,what", NEGCTL_BLIND)
def test_negctl_blind_spots_are_known(portlib, vrunner, function, mode, what):
    """A blind spot that starts reading RED means the corpus grew: move it to NEGCTL_RED. `caller:function` runs the
    caller's vectors."""
    caller, _, fn = function.rpartition(":")
    if fn == "updateWalkers":
        needs(portlib, "pathStep")
    portlib.negctl(fn, mode)
    assert vrunner.run_function(caller or fn).verdict == "GREEN", f"{function} mode {mode} ({what}) is no longer blind"


def test_every_function_has_a_negctl(portlib):
    funcs = portlib.functions()
    for f in FUNCTIONS:
        assert f in funcs, f
        assert portlib.negctl(f, 0) == 0, f


# ------------------------------------------------------------------------------------------- shot tracks
@pytest.mark.parametrize("mode", [0, 1, 2])
def test_track_point_cap(portlib, mode):
    """The open side +0x8a (count >= 31): point 31 is written with the ball position and the count stays 31."""
    tr, gf = off(portlib, "g_storyPairs"), off(portlib, "g_golfers")
    portlib.negctl("appendPairTrackPoint", mode)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(tr + 2 * TRACK, struct.pack("<hhh", 7, 9, 31))
        st.write(gf + 7 * GOLFER + 0xd4, struct.pack("<iii", 0x1234, -0x5678, 0x99))
        st.call("appendPairTrackPoint", [2])
        cnt = struct.unpack("<h", st.read(tr + 2 * TRACK + 4, 2))[0]
        pts = [struct.unpack("<i", st.read(tr + 2 * TRACK + base + 31 * 4, 4))[0] for base in (8, 0x88, 0x108)]
    assert pts == [0x1234, -0x5678, 0x99]
    assert (cnt == 31) == (mode != 2)


@pytest.mark.parametrize("mode", [0, 2])
def test_add_golfer_pair_slots(portlib, mode):
    """thin coverage: a full table writes nothing; the first free slot (here 3, with a stale count) is claimed."""
    tr, gf = off(portlib, "g_storyPairs"), off(portlib, "g_golfers")
    portlib.negctl("addGolferPair", mode)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        for k in range(16):
            st.write(tr + k * TRACK, struct.pack("<h", k))
        before = st.image()
        st.call("addGolferPair", [5])
        assert st.image() == before, "full table: nothing written"
        st.write(tr + 3 * TRACK, struct.pack("<hhh", -1, 0, 17))
        st.write(gf + 5 * GOLFER, bytes(range(256)))
        st.write(gf + 5 * GOLFER + 0xa2, struct.pack("<h", 6))
        st.write(gf + 6 * GOLFER, b"\x5a" * 256)
        st.call("addGolferPair", [5])
        g, p, cnt = struct.unpack("<hhh", st.read(tr + 3 * TRACK, 6))
        snap_g, snap_p = st.read(tr + 3 * TRACK + 0x188, 256), st.read(tr + 3 * TRACK + 0x288, 256)
    assert (g, p) == (5, 6)
    assert (cnt == 1) == (mode == 0), "count reset to 0 then point 0 appended"
    assert snap_p == b"\x5a" * 256 and snap_g[:4] == bytes(range(4))


@pytest.mark.parametrize("mode", [0, 1])
def test_reset_golfer_clears_hole(portlib, mode):
    gf = off(portlib, "g_golfers")
    portlib.negctl("resetGolfer", mode)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(gf + 4 * GOLFER + 0x1d, b"\x03\x00\x00\x00\x09\x05")
        st.call("resetGolfer", [4])
        b = st.read(gf + 4 * GOLFER + 0x1d, 6)
    assert b[0] == 0 and b[5] == 0
    assert (b[4] == 0) == (mode == 0)


# ------------------------------------------------------------------------------------------- spawning
def test_spawn_object_no_free_cell_never_returns(portlib):
    """No free occupancy cell in rows / columns 0..3: the original's draw loop never ends; the port reports it."""
    an, rb = off(portlib, "g_animalRecords"), off(portlib, "g_recordBanks")
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(an + 0x20, struct.pack("<h", -1))
        st.write(rb + 2 * 0x74 + 0x50, b"\x01" * 36)
        with pytest.raises(L.SgError) as e:
            st.call("spawnObject", [2, 0, 1, 1])
        assert e.value.code == L.SG_E_NO_RETURN
        # record 0 was claimed before the loop (as in the original), so the next spawn takes record 1
        assert struct.unpack("<h", st.read(an + 0x20, 2))[0] == 0
        st.write(an + 0x3c + 0x20, struct.pack("<h", -1))
        st.write(rb + 2 * 0x74 + 0x50 + 3 * 6 + 2, b"\xff")
        st.call("spawnObject", [2, 0, 1, 1])
        sx, sy = struct.unpack("<ii", st.read(an + 0x3c + 0x30, 8))
    assert (sx, sy) == (((3 << 10) + 0x200) // 2, ((2 << 10) + 0x200) // 2)


def test_spawn_walker_has_no_bound(portlib):
    """model_defects#11: with all 64 slots taken the search runs past the table and claims the bytes after it."""
    wk = off(portlib, "g_walkers")
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        for k in range(64):
            st.write(wk + k * 0x4c + 0x12, b"\x01")
        assert st.call("spawnWalker", [-2]).eax == 64
        assert st.read(wk + 64 * 0x4c + 0x12, 2) == b"\x01\xfe"


# ------------------------------------------------------------------------------------------- state-test helpers
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")


class S:
    """Named access to a State's GameState bytes."""

    def __init__(self, lib, st):
        self.st, self.o = st, {n: o for n, o, s in lib.objects()}

    def w(self, sym, off, fmt, *v):
        self.st.write(self.o[sym] + off, struct.pack("<" + fmt, *v))

    def r(self, sym, off, fmt):
        n = struct.calcsize("<" + fmt)
        v = struct.unpack("<" + fmt, self.st.read(self.o[sym] + off, n))
        return v[0] if len(v) == 1 else v


def needs(lib, *names):
    missing = [n for n in names if n not in lib.functions()]
    if missing:
        pytest.skip(f"peer function(s) not registered yet: {missing}")


def _animal_on_screen(s, frame, cols):
    """Animal 0 at tile (1, 1) + (0x200, 0x200), camera (0, 0), zoom 4, 800 x 600, rotation 0, tile type 0 flagged
    TTA_HEIGHT_3 (no height lookup): worldToScreen gives (496, 300), on screen."""
    s.w("g_zoomScale", 0, "i", 4)
    s.w("g_screenWidth", 0, "i", 800)
    s.w("g_screenHeight", 0, "i", 600)
    s.w("g_tileTypeAttrs", 0x2c, "I", 8)
    s.w("g_animalRecords", 0x20, "hhhhhhh", 3, 0, 0, 1, 1, 0, 0x140)
    s.w("g_animalRecords", 0x30, "iibbb", 0x200, 0x200, 0, 0, frame)
    s.w("g_sheetCols", 4 * (3 + 0x140), "i", cols)


@pytest.mark.parametrize("mode", [0, 1])
def test_place_animal_on_screen(portlib, mode):
    """coverage none: the on-screen arm: frame % count, advance and wrap, g_textBuffer = x digits, separator, y digits."""
    portlib.negctl("placeAnimal", mode)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        s = S(portlib, st)
        _animal_on_screen(s, 7, 4)
        s.w("g_flags", 0, "I", 4)                       # paused: no advance, no rolls
        st.call("placeAnimal", [0])
        assert s.r("g_animalRecords", 0x3a, "b") == 3   # 7 % 4, not advanced
        assert st.read(s.o["g_textBuffer"], 3) == b"11\0"   # the separator pool is empty in a zero state
        s.w("g_flags", 0, "I", 0)
        s.w("g_animalRecords", 0x2a, "h", 5)            # waiting: the wait arm returns before the rolls
        st.call("placeAnimal", [0])
        frame = s.r("g_animalRecords", 0x3a, "b")
        assert s.r("g_animalRecords", 0x2a, "h") == 4 and s.r("g_animalRecords", 0x2c, "h") == 0x140
    assert frame == (0 if mode == 0 else 4), "3 + 1 wraps at the frame count 4"


def test_place_animal_zero_frame_count_faults(portlib):
    """The original's idiv by the sprite's frame count: 0 raises #DE."""
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        _animal_on_screen(S(portlib, st), 1, 0)
        with pytest.raises(L.SgError) as e:
            st.call("placeAnimal", [0])
        assert e.value.code == L.SG_E_DIVIDE


@need_exe
@pytest.mark.parametrize("mode", [0, 2])
def test_place_animal_walk_step(portlib, mode):
    """The step arm: off screen and paused (no rolls), state 0x15a, two steps left: the sub-tile position moves by the
    heading's direction entry * 1024 / 32 and one step is used; at the last step the state returns to 0x140."""
    portlib.exe_load()
    dx = struct.unpack("<8i", portlib.exe_const("f_tab_dirDx_4c2878"))
    dy = struct.unpack("<8i", portlib.exe_const("f_tab_dirDy_4c2898"))
    portlib.negctl("placeAnimal", mode)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.fill_from_exe()
        s = S(portlib, st)
        s.w("g_flags", 0, "I", 4)
        s.w("g_animalRecords", 0x2a, "hh", 0, 0x15a)
        s.w("g_animalRecords", 0x30, "iiBBb", 0x300, 0x100, 3, 2, 0)
        st.call("placeAnimal", [0])
        assert s.r("g_animalRecords", 0x30, "ii") == (0x300 + int((dx[3] << 10) / 32), 0x100 + int((dy[3] << 10) / 32))
        assert s.r("g_animalRecords", 0x39, "B") == 1 and s.r("g_animalRecords", 0x2c, "h") == 0x15a
        st.call("placeAnimal", [0])
        assert s.r("g_animalRecords", 0x39, "B") == 0 and s.r("g_animalRecords", 0x2c, "h") == 0x140


@pytest.mark.parametrize("length,flags,par", [(0x272, 0, 6), (0x271, 0, 5), (0x1db, 0, 5), (0xfa, 0, 4), (0xe1, 0x60, 3),
                                              (0x1c5, 0x60, 5), (0x1c5, 0, 4), (0x32, 0, 2), (0x33, 0, 3)])
def test_reset_golfer_state_par(portlib, length, flags, par):
    """coverage none: the par of g_holesPlusOne's record from its length (flags 0x60 add 0x19 above 0xfa; above 0x12c
    25 * level is subtracted); the golfer types come back from their backup; g_tournamentPrep = 0."""
    needs(portlib, "updateMembership")
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        s = S(portlib, st)
        s.w("g_holesPlusOne", 0, "i", 7)
        s.w("g_holes", 7 * 0x208, "b", 1)
        s.w("g_holes", 7 * 0x208 + 4, "h", length)
        s.w("g_holes", 7 * 0x208 + 0x200, "I", flags)
        s.w("g_golferTypesBackup", 0x30, "B", 0x5a)
        s.w("g_tournamentPrep", 0, "i", 9)
        st.call("resetGolferState", [])
        assert s.r("g_holes", 7 * 0x208, "b") == par
        assert s.r("g_golferTypes", 0x30, "B") == 0x5a and s.r("g_tournamentPrep", 0, "i") == 0


def test_reset_golfer_state_negctl(portlib):
    needs(portlib, "updateMembership")
    portlib.negctl("resetGolferState", 1)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        s = S(portlib, st)
        s.w("g_holes", 0, "bxxxh", 1, 0x271)       # g_holesPlusOne 0: record 0
        st.call("resetGolferState", [])
        assert s.r("g_holes", 0, "b") == 6, "the wrong body gives par 6 at length 0x271"


@need_exe
@pytest.mark.parametrize("mode", [0, 1])
def test_compose_golfer_sprite_write_back(portlib, mode):
    """coverage none (drawing): the simulation effect is the write-back of the look indices into the type record."""
    portlib.exe_load()
    quotes = portlib.exe_const("f_tab_quotes_4d55e8")
    portlib.negctl("composeGolferSprite", mode)
    t = 12
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.fill_from_exe()
        s = S(portlib, st)
        s.w("g_golfers", 0xb6, "h", t)
        s.w("g_golfers", 0x19, "B", 4)
        T = t * 0x230
        s.w("g_golferTypes", T + 0x21, "BbB", 0, 0x13, 0x90)    # demographics, body 0x13 (no table), look_23 0x90
        s.w("g_golferTypes", T + 0x2c, "I", 0)
        st.call("composeGolferSprite", [0])
        got = s.r("g_golferTypes", T + 0x23, "Bbbbbb")
    ev = struct.unpack("b", quotes[0x13 * 68 + 3:0x13 * 68 + 4])[0]
    a, d = 3 * t % 10, t % 10
    exp = (0x90 | a, a, d, 0, 0, ev) if mode == 0 else (0x90 | a, d, a, 0, 0, ev)
    assert got == exp


def test_load_golf_pro_faults(portlib):
    """loadGolfPro divides by 4 - g_difficulty (difficulty 4 raises #DE), and with no filled pro record 1..99 the draw
    loop never ends."""
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        s = S(portlib, st)
        s.w("g_holes", 0x208, "b", 1)                    # hole 1 open: golfers 0 and 1
        s.w("g_difficulty", 0, "i", 4)
        with pytest.raises(L.SgError) as e:
            st.call("loadGolfPro", [0])
        assert e.value.code == L.SG_E_DIVIDE
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        s = S(portlib, st)
        s.w("g_holes", 0x208, "b", 1)
        for k in range(100):
            s.w("g_golfPros", k * 0x38 + 0x20, "b", -1)
        with pytest.raises(L.SgError) as e:
            st.call("loadGolfPro", [0])
        assert e.value.code == L.SG_E_NO_RETURN


@pytest.mark.parametrize("mode", [0, 1])
def test_update_walkers_sale(portlib, mode):
    """partial_branch: the sale arm (action 0xfb at timer 4) adds 2 to the cash, clears the golfer's +0xa8 meter, books 2
    to this year's food and drink and pops the money label; the walker then waits."""
    needs(portlib, "emitGolferThought")
    portlib.negctl("updateWalkers", mode)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        s = S(portlib, st)
        s.w("g_playerGolfer", 0, "i", -1)
        s.w("g_walkers", 0x12, "BbBB", 1, -5, 0xfb, 5)
        s.w("g_walkers", 0x1a, "h", 5)
        s.w("g_walkers", 0x20, "h", 3)
        s.w("g_golfers", 3 * 0x100 + 0xa8, "h", 0x40)
        s.w("g_year", 0, "h", 2003)
        st.call("updateWalkers", [0])
        assert s.r("g_cash_div100", 0, "i") == (2 if mode == 0 else 3)
        assert s.r("g_golfers", 3 * 0x100 + 0xa8, "h") == 0
        assert s.r("g_financeYears", 3 * 20 + 4, "h") == 2
        assert s.r("g_walkers", 0x15, "B") == 4 and s.r("g_walkers", 0x1a, "h") == 4
        if "pointsPopup" not in portlib.functions():
            assert s.r("g_moneyLabelValue", 0, "i") == 2


def test_add_golfer_to_table_full(portlib):
    """one_sided: no free ball record (every x != -1 in a zero state): one range draw per record, 256 returned."""
    needs(portlib, "holeQuadrants")
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        s = S(portlib, st)
        s.w("g_golfers", 0xd4, "ii", 0x1234, 0x5678)
        assert st.call("addGolferToTable", [0]).eax == 256
        assert s.r("g_ballRecords", 0, "ii") == (0, 0)
        assert s.r("g_rng", 0, "I") != 0


@need_exe
def test_place_habitat_success_path(portlib):
    """partial_branch (the vectors all leave at the 1-in-8 roll): over seeds until habitats are placed, check what the
    success path guarantees: the first new slot lies within 1 tile of (x, y) on a tile whose type is the animal record's
    +0x10 byte with course-type bit 0 in its +0x11 mask, state -20; the cluster slots link to it; the message queue got the
    record's name."""
    portlib.exe_load()
    animals = portlib.exe_const("f_tab_animals_4c1998")
    placed = 0
    for seed in range(1, 300):
        with portlib.state() as st:
            st.set_data_base(VL.DATA_VA)
            st.fill_from_exe()
            s = S(portlib, st)
            for k in range(128):
                s.w("g_habitatSlots", k * 0x14 + 0x12, "B", 0xff)
            s.w("g_preferences", 0, "I", 0x20)
            b10 = animals[0x10]
            st.write(s.o["g_tileType"], bytes([b10]) * 2500)
            s.w("g_rng", 0, "I", seed * 7919)
            st.call("placeHabitat", [20, 21, b10])
            if s.r("g_habitatSlots", 0x12, "B") == 0xff:
                continue
            placed += 1
            k = s.r("g_habitatSlots", 0x12, "B")
            assert animals[k * 18 + 0x10] == b10 and (animals[k * 18 + 0x11] & 1)
            x, y, state = s.r("g_habitatSlots", 0, "iii")
            assert abs((x >> 10) - 20) <= 1 and abs((y >> 10) - 21) <= 1 and state == -20
            for j in range(1, 128):
                if s.r("g_habitatSlots", j * 0x14 + 0x12, "B") == 0xff:
                    break
                assert s.r("g_habitatSlots", j * 0x14 + 0xc, "h") == 0
            name = animals[k * 18:k * 18 + 16].split(b"\0")[0]
            if "queueMessage" not in portlib.functions():
                assert st.read(s.o["g_labelQueueText"], len(name)) == name
    assert placed > 0, "no seed reached the success path"
