"""S6.4: the `fixed`-profile switches of the golfers module (placeHabitat.clusterLoopBound, spawnObject.cellSearchBound,
loadGolfPro.divideAndRejectGuard, placeAnimal.zeroFrameCount). Default 1 = the original (NoReturn / DivideError),
0 = a clean return. Per switch: (1) ON the fault / hang state still raises, (2) OFF it returns, (3) a completing control gives
the same whole-state result with the switch ON and OFF, (4) a wrong guard (negctl) changes the completing control (RED).
Notes: log/roadmap/S6/4_notes.md."""
from __future__ import annotations

import hashlib
import struct

import pytest

from port.test import lib as L
from port.test import test_golfers as TG
from port.test.test_golfers import S, VL, need_exe

pytestmark = pytest.mark.port

SWITCHES = ["placeHabitat.clusterLoopBound", "spawnObject.cellSearchBound", "loadGolfPro.divideAndRejectGuard",
            "placeAnimal.zeroFrameCount"]


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    portlib.negctl_reset()
    yield
    portlib.defect_reset()
    portlib.negctl_reset()


def image_hash(lib, st):
    h = hashlib.sha256()
    for n, o, size in lib.objects():
        h.update(st.read(o, size))
    return h.hexdigest()


def test_switches_exist_and_default_on(portlib):
    d = portlib.defects()
    for n in SWITCHES:
        assert n in d and d[n]["on"] == 1, n


# ------------------------------------------------------------------------------------------- placeHabitat (D8, D40a, D40b)
def _habitat_state(lib, st, free_slot, seed, byte_after):
    """Every habitat slot taken but `free_slot`; the byte after the table (slot 128's kind) = byte_after."""
    s = S(lib, st)
    for k in range(128):
        s.w("g_habitatSlots", k * 0x14 + 0x12, "B", 0xff if k in free_slot else 0)
    st.write(s.o["g_habitatSlots"] + 128 * 0x14 + 0x12, bytes([byte_after]))
    s.w("g_preferences", 0, "I", 0x20)
    st.write(s.o["g_tileType"], bytes([lib.exe_const("f_tab_animals_4c1998")[0x10]]) * 2500)
    s.w("g_rng", 0, "I", seed * 7919)
    return s


def _habitat_call(lib, free_slot, byte_after, defect, seed_range=range(1, 400), want=0):
    """Per seed: (seed, outcome, whole-image hash)."""
    lib.exe_load()
    b10 = lib.exe_const("f_tab_animals_4c1998")[0x10]
    out = []
    for seed in seed_range:
        lib.defect("placeHabitat.clusterLoopBound", defect)
        with lib.state() as st:
            st.set_data_base(VL.DATA_VA)
            st.fill_from_exe()
            s = _habitat_state(lib, st, free_slot, seed, byte_after)
            try:
                st.call("placeHabitat", [20, 21, b10])
                res = "ok"
            except L.SgError as e:
                res = e.code
            # claimed = the number of slots that were free and are taken now (the first addHabitatSlot ran)
            claimed = sum(st.read(s.o["g_habitatSlots"] + k * 0x14 + 0x12, 1)[0] != 0xff for k in free_slot)
            out.append((seed, res, image_hash(lib, st), claimed))
            if want and sum(r[3] == len(free_slot) for r in out) >= want:
                break
    return out


@need_exe
def test_place_habitat_table_full_slot_127(portlib):
    on = _habitat_call(portlib, (127,), 0xff, 1)
    off = _habitat_call(portlib, (127,), 0xff, 0)
    hung = [a for a, b in zip(on, off) if a[1] == L.SG_E_NO_RETURN]
    assert hung, "no seed reached the hang (the probe d40_habitat needs the loop entered with c < 3)"
    for a, b in zip(on, off):
        if a[1] == L.SG_E_NO_RETURN:
            assert b[1] == "ok"       # OFF: returns cleanly
        else:
            assert a[1] == b[1] == "ok" and a[2] == b[2]   # a seed that completes ON is identical OFF


@need_exe
def test_place_habitat_completing_control(portlib):
    """Slots 125 and 126 free: the loop's first addHabitatSlot takes 126 and it ends. Equal ON / OFF."""
    on = _habitat_call(portlib, (125, 126), 0x00, 1, range(1, 30000), want=3)
    off = _habitat_call(portlib, (125, 126), 0x00, 0, range(1, 30000), want=3)
    assert on == off
    assert any(r[3] == 2 for r in on), "no seed ran the loop"
    assert all(r[1] == "ok" for r in on)


@need_exe
def test_place_habitat_wrong_guard_reads_red(portlib):
    """Mode 2 skips the loop for slot 126 as well: slots 126 and 127 free: the original runs the loop once and claims 127."""
    seeds = range(1, 30000)
    base = _habitat_call(portlib, (126, 127), 0xff, 0, seeds, want=3)
    assert any(r[3] == 2 for r in base), "no seed ran the loop"
    portlib.negctl("placeHabitat", 2)
    bad = _habitat_call(portlib, (126, 127), 0xff, 0, seeds, want=3)
    assert base != bad


# ------------------------------------------------------------------------------------------- spawnObject (D41)
def _spawn(lib, free_cells, defect, mode=0):
    lib.defect("spawnObject.cellSearchBound", defect)
    lib.negctl("spawnObject", mode)
    an, rb = TG.off(lib, "g_animalRecords"), TG.off(lib, "g_recordBanks")
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(an + 0x20, struct.pack("<h", -1))
        cells = bytearray(b"\x01" * 36)
        for (i, j) in free_cells:
            cells[i * 6 + j] = 0xff
        st.write(rb + 2 * 0x74 + 0x50, bytes(cells))
        st.write(TG.off(lib, "g_rng"), struct.pack("<I", 12345))
        try:
            st.call("spawnObject", [2, 0, 1, 1])
            res = "ok"
        except L.SgError as e:
            res = e.code
        return res, image_hash(lib, st)


def test_spawn_object_full_bank(portlib):
    assert _spawn(portlib, [], 1)[0] == L.SG_E_NO_RETURN
    res, h = _spawn(portlib, [], 0)
    assert res == "ok"
    # nothing claimed, nothing drawn: identical to a state where no animal record was free
    an = TG.off(portlib, "g_animalRecords")
    portlib.defect("spawnObject.cellSearchBound", 0)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.write(an + 0x20, struct.pack("<h", -1))
        rb = TG.off(portlib, "g_recordBanks")
        st.write(rb + 2 * 0x74 + 0x50, b"\x01" * 36)
        st.call("spawnObject", [2, 0, 1, 1])
        assert struct.unpack("<h", st.read(an + 0x20, 2))[0] == -1   # record 0 still free
        assert struct.unpack("<I", st.read(TG.off(portlib, "g_rng"), 4))[0] == 0   # no draw


@pytest.mark.parametrize("cell", [(0, 0), (2, 3), (3, 3)])
def test_spawn_object_completing_control(portlib, cell):
    assert _spawn(portlib, [cell], 1) == _spawn(portlib, [cell], 0)


def test_spawn_object_wrong_guard_reads_red(portlib):
    good = _spawn(portlib, [(2, 3)], 0)
    assert good[0] == "ok"
    assert _spawn(portlib, [(2, 3)], 0, mode=3) != good


# ------------------------------------------------------------------------------------------- loadGolfPro (D42a, D42b)
def _pro(lib, difficulty, filled, defect, mode=0, seed=777):
    lib.defect("loadGolfPro.divideAndRejectGuard", defect)
    lib.negctl("loadGolfPro", mode)
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        s = S(lib, st)
        s.w("g_holes", 0x208, "b", 1)
        s.w("g_difficulty", 0, "i", difficulty)
        s.w("g_rng", 0, "I", seed)
        for k in range(100):
            s.w("g_golfPros", k * 0x38 + 0x20, "b", 0 if k in filled else -1)
            s.w("g_golfPros", k * 0x38 + 0x36, "h", 10 if k in filled else 0)
        try:
            r = st.call("loadGolfPro", [0]).eax
            res = ("ok", r)
        except L.SgError as e:
            res = e.code
        return res, image_hash(lib, st)


FULL = set(range(1, 100))


@pytest.mark.parametrize("difficulty,code", [(4, L.SG_E_DIVIDE), (-4, L.SG_E_DIVIDE)])
def test_load_golf_pro_zero_divisor(portlib, difficulty, code):
    assert _pro(portlib, difficulty, FULL, 1)[0] == code
    res, _ = _pro(portlib, difficulty, FULL, 0)
    assert res[0] == "ok"


@pytest.mark.parametrize("filled", [set(), {0}])
def test_load_golf_pro_empty_roster(portlib, filled):
    assert _pro(portlib, 2, filled, 1)[0] == L.SG_E_NO_RETURN
    assert _pro(portlib, 2, filled, 0)[0][0] == "ok"


@pytest.mark.parametrize("difficulty", [0, 1, 2, 3, -1])
def test_load_golf_pro_completing_control(portlib, difficulty):
    a, b = _pro(portlib, difficulty, FULL, 1), _pro(portlib, difficulty, FULL, 0)
    assert a == b
    assert a[0][0] == "ok" if isinstance(a[0], tuple) else False, f"control must complete: {a[0]}"


def test_load_golf_pro_one_record_roster_completes_equal(portlib):
    assert _pro(portlib, 2, {7}, 1) == _pro(portlib, 2, {7}, 0)


def test_load_golf_pro_wrong_guard_reads_red(portlib):
    good = _pro(portlib, 3, FULL, 0)
    assert _pro(portlib, 3, FULL, 0, mode=2) != good


# ------------------------------------------------------------------------------------------- placeAnimal (D43)
def _animal(lib, cols, defect, mode=0, frame=1):
    lib.defect("placeAnimal.zeroFrameCount", defect)
    lib.negctl("placeAnimal", mode)
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        TG._animal_on_screen(S(lib, st), frame, cols)
        try:
            st.call("placeAnimal", [0])
            res = "ok"
        except L.SgError as e:
            res = e.code
        return res, image_hash(lib, st)


def test_place_animal_zero_frame_count(portlib):
    assert _animal(portlib, 0, 1)[0] == L.SG_E_DIVIDE
    res, h = _animal(portlib, 0, 0)
    assert res == "ok"
    # nothing was written: the state equals the untouched fixture
    lib = portlib
    with lib.state() as st:
        st.set_data_base(VL.DATA_VA)
        TG._animal_on_screen(S(lib, st), 1, 0)
        assert image_hash(lib, st) == h


@pytest.mark.parametrize("cols", [1, 4, 6])
def test_place_animal_completing_control(portlib, cols):
    a = _animal(portlib, cols, 1)
    assert a[0] == "ok" and a == _animal(portlib, cols, 0)


def test_place_animal_wrong_guard_reads_red(portlib):
    good = _animal(portlib, 1, 0)
    assert _animal(portlib, 1, 0, mode=3) != good
