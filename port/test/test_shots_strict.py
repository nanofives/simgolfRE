"""U-1012: chooseShotTarget.strictFlagSticky (model_defects#37) on constructed GameStates, judged against the ORIGINAL.

The original keeps the strict flag of its landing scans in one frame word that nothing clears: the first landing that
meets the strict condition stores 1 there and every later strict scan of the same search, for other cells and for the
next pass, runs with 1 (scanTileLine then scores the last cells of a line only when they are collar type). The port's
switch `chooseShotTarget.strictFlagSticky` = 1 keeps that, 0 recomputes the flag per landing. No vector and no segment
reaches a search with two landings of which the second fails the condition, so the switch had no evidence (U-1012).

Here a deterministic family of states (a random tile grid, one golfer with the slope preference bit, a hole 3 to 13 tiles
from the ball, `_case`) is built in the port. The pinned seeds are states where switching the flag OFF changes the search's
result. The evidence, in three layers:

* default suite: the port with the switch ON equals the SHA-256 digests of the ORIGINAL's result (eax and the whole
  GameState image after the call, run in Unicorn when the digests were pinned); the port with the switch OFF does not.
* opt-in `SG_EMU_ORACLE=1` (needs unicorn and the user's original/golf_clean.exe): the original runs again over each state
  (re/tools/vector_emu.py's image and CPU setup, a lean runner without per-access hooks), the port ON equals it byte for
  byte, the port OFF differs, and code hooks on the two instructions of the original's strict flag (the store and the
  read that feeds scanTileLine) show the mechanism: a scan that reads 1 although its own landing did not store it.
* controls: the switch OFF is the control (the fixed behaviour must read RED); chooseShotTarget's own negative-control
  modes are judged by the same digests.

What the oracle leaves out, both sides alike (U-1160, U-1161): the original's frame word starts at 0 (the emulated stack
is zero-filled; in the live game it is whatever an earlier frame left, the port starts it at 0, U-0880) and the message
pump the original calls at the end of every pruning step (pumpInput483bd0) is replaced by a `ret`, as the port drops it
(drawing and input, U-0881): with no display object in the state the real pump dereferences a null display.
"""
from __future__ import annotations

import hashlib
import math
import os
import pathlib
import random
import struct
import sys

import pytest

from port.test import lib as L

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

pytestmark = pytest.mark.port
need_exe = pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") != "1",
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

DEFECT = "chooseShotTarget.strictFlagSticky"
FUNCTION = "chooseShotTarget"
G = 5                       # the golfer the search runs for
ENTRY = 0x00422FB0          # chooseShotTarget
PUMP = 0x00483BD0           # pumpInput483bd0 (UI): replaced by `ret` in the emulated original, dropped by the port
STRICT_STORE = 0x00423B0E   # mov dword [esp + 0xb4], 1   (the only write of the strict flag)
STRICT_READ = 0x00423B20    # mov edx, [esp + 0xb4]       (feeds scanTileLine's seventh argument)
FRAME_STRICT = 0xB4

# the pinned states: seeds of `_case` where the switch changes eax and / or the image (found by a sweep of 700 seeds, 42
# differ; these eight cover the three changes of eax: aim 0 -> 1, 1 -> -1, -1 -> 0 and the image-only ones)
SEEDS = [4, 35, 66, 156, 182, 239, 394, 526]

# SHA-256 (first 32 hex digits) of eax (int32 LE) + the GameState image after the ORIGINAL's call; regenerate with
# `SG_EMU_ORACLE=update pytest port/test/test_shots_strict.py -k update -s` after the oracle tests are GREEN
GOLDEN: dict[int, str] = {
    4: "272ba69e785c3399d3488e39dd41f561",
    35: "5915ec9396b0320b7d5a7c5bd773b22b",
    66: "ee52fdecd8eaaad028362e035e488331",
    156: "c7e2545491c78a8444a5e10c515a2f05",
    182: "5da754bab6d963c8498eaa98e9d8f910",
    239: "c22b91fb42795488f5a57e5772d71ee5",
    394: "7e0f5ec1a2995049f8254de310d28d59",
    526: "83a552295fa6a0eb1a1a86900a3914cd",
}

# chooseShotTarget's own negative-control modes (port/src/sim/shots.cpp g_negctl_choose_shot_target): each RED mode
# changes the result of at least one pinned state
NEGCTL_RED = {1: "1 trial per cell in the first pass", 2: "variant 1 (cell corner) never tried",
              3: "hole-distance divisor always 4", 4: "landing value weight 4 instead of 8"}
# blind on the pinned states: the prune tolerance 0x40 instead of 0x80 changes nothing here (no cell of these searches
# scores between best + 0x40 and best + 0x80 after a pass); a state whose second pass keeps cells in that band would judge it
NEGCTL_BLIND = {5: "prune tolerance starts at 0x40"}

# -------------------------------------------------------------------------------------------------------- the states
def _objs(lib) -> dict[str, tuple[int, int]]:
    return {n: (o, s) for n, o, s in lib.objects()}


@pytest.fixture(scope="module")
def base(portlib):
    """The user's initialised data image with the tile-type table copied from its init copy and the sine table built."""
    if not L.exe_path().exists():
        pytest.skip("the user's golf_clean.exe is not present")
    assert portlib.exe_load() == ""
    o = _objs(portlib)
    with portlib.state() as st:
        st.set_data_base(VL.DATA_VA)
        st.fill_from_exe(whole_image=True)
        img = bytearray(st.image())
        init, a = o["f_tab_tileTypeAttrInit_4c1a40"][0], o["g_tileTypeAttrs"][0]
        img[a:a + 23 * 0x30] = img[init:init + 23 * 0x30]
        st.load_image(bytes(img))
        st.call("initSinTable", [])
        return st.image()


def _case(seed: int) -> dict:
    """The parameters of one state: ball and hole tiles, tile grid, the golfer's slope preference and skill level. The
    draw order is the one of the sweep that found the pinned seeds (the ten per-skill draws are skipped, not used)."""
    rng = random.Random(seed)
    ball = (rng.randrange(12, 38), rng.randrange(12, 38))
    ang = rng.random() * 6.28
    d = rng.randrange(3, 14)
    hole = (max(1, min(48, int(ball[0] + d * math.cos(ang)))), max(1, min(48, int(ball[1] + d * math.sin(ang)))))
    tiles = rng.choice([(2, 2, 2, 4, 4, 5, 7, 9, 17, 13), (2, 2, 2, 2, 4, 7), (2, 4, 5, 9, 17, 10, 13, 7, 3, 8)])
    prefs, kind = rng.choice([4, 5, 6, 7]), rng.choice([0, 0, 1])
    grid = bytes(rng.choice(tiles) for _ in range(2500))
    difficulty, rng_seed = rng.randrange(3), rng.getrandbits(32)
    for _ in range(10):
        rng.randrange(6, 12)
    return {"ball": ball, "hole": hole, "grid": grid, "prefs": prefs, "kind": kind, "difficulty": difficulty,
            "rng": rng_seed, "skill": rng.randrange(8, 30)}


def _state_image(portlib, base: bytes, seed: int) -> bytes:
    c = _case(seed)
    o = _objs(portlib)
    img = bytearray(base)

    def put(name, data, at=0):
        img[o[name][0] + at:o[name][0] + at + len(data)] = data

    put("g_tileType", c["grid"])
    put("g_tileFlags", bytes(5000))
    put("g_difficulty", struct.pack("<i", c["difficulty"]))
    put("g_rng", struct.pack("<I", c["rng"]))
    rec = bytearray(0x100)
    rec[0x18], rec[0x19] = c["kind"], c["prefs"]
    rec[0xF0:0xFA] = bytes([c["skill"]]) * 10
    bx, by = c["ball"]
    struct.pack_into("<ii", rec, 0xD4, (bx << 10) + 512, (by << 10) + 512)    # the ball
    struct.pack_into("<ii", rec, 0x00, (bx << 10) + 512, (by << 10) + 512)    # the golfer
    put("g_golfers", bytes(rec), 0x100 * G)
    put("g_holes", struct.pack("<ii", *c["hole"]), 0x18)
    return bytes(img)


# ------------------------------------------------------------------------------------------------------------ the port
def _port(portlib, image: bytes, on: bool, negctl: int = 0):
    """(eax, image after) of chooseShotTarget with the switch ON / OFF (and a negative-control mode)."""
    try:
        portlib.defect(DEFECT, on)
        if negctl:
            portlib.negctl(FUNCTION, negctl)
        with portlib.state() as st:
            st.set_data_base(VL.DATA_VA)
            st.load_image(image)
            eax = st.call(FUNCTION, [G]).eax
            return eax, st.image()
    finally:
        portlib.negctl_reset()
        portlib.defect(DEFECT, True)


def _digest(eax: int, image: bytes) -> str:
    return hashlib.sha256(struct.pack("<I", eax & 0xFFFFFFFF) + image).hexdigest()[:32]


def _diff_symbols(portlib, a: bytes, b: bytes, limit: int = 12) -> list[str]:
    """The GameState objects whose bytes differ, as name+first offset."""
    out = []
    for name, off, size in sorted(portlib.objects(), key=lambda t: t[1]):
        if a[off:off + size] != b[off:off + size]:
            k = next(i for i in range(size) if a[off + i] != b[off + i])
            out.append(f"{name}+0x{k:x}")
            if len(out) == limit:
                break
    return out


# ------------------------------------------------------------------------------------------------ the original, Unicorn
def run_original(image: bytes, watch: dict[int, str] | None = None):
    """The ORIGINAL chooseShotTarget(G) over `image` (the GameState at its original data base) in Unicorn.

    Returns (eax, error or None, image after, events). `watch` maps instruction addresses to event names; the hook
    records (name, dword at [esp + FRAME_STRICT] when the name is 'read'). pumpInput483bd0 is replaced by a `ret`."""
    import vector_emu as VE
    from unicorn import (UC_ARCH_X86, UC_HOOK_CODE, UC_HOOK_MEM_FETCH_UNMAPPED, UC_HOOK_MEM_READ_UNMAPPED,
                         UC_HOOK_MEM_WRITE_UNMAPPED, UC_MODE_32, Uc, UcError)
    from unicorn.x86_const import (UC_X86_REG_DS, UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ES, UC_X86_REG_ESP,
                                   UC_X86_REG_FPCW, UC_X86_REG_FS, UC_X86_REG_GDTR, UC_X86_REG_GS, UC_X86_REG_SS)
    img = VE.load_image()
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(VL.IMAGE_BASE, VE.IMAGE_END - VL.IMAGE_BASE)
    uc.mem_write(VL.IMAGE_BASE, img.data)
    uc.mem_write(PUMP, b"\xC3")
    uc.mem_write(VL.DATA_VA, image)
    for base, size in ((VE.STACK_BASE, VE.STACK_SIZE), (VE.SENTINEL, VE.PAGE), (VE.TEB, VE.PAGE), (VE.GDT, VE.PAGE)):
        uc.mem_map(base, size)
    teb = struct.pack("<IIII", 0xFFFFFFFF, VE.STACK_BASE + VE.STACK_SIZE, VE.STACK_BASE, 0) + b"\0" * 8
    uc.mem_write(VE.TEB, teb + struct.pack("<I", VE.TEB))
    for slot, args in ((1, (0, 0xFFFFF, 0xFB, 0xC)), (2, (0, 0xFFFFF, 0xF3, 0xC)), (3, (VE.TEB, 0xFFF, 0xF3, 0x4)),
                       (4, (0, 0xFFFFF, 0x93, 0xC))):
        uc.mem_write(VE.GDT + slot * 8, VE._gdt_entry(*args))
    uc.reg_write(UC_X86_REG_GDTR, (0, VE.GDT, VE.PAGE - 1, 0))
    for reg, sel in ((UC_X86_REG_DS, 0x13), (UC_X86_REG_ES, 0x13), (UC_X86_REG_GS, 0x13), (UC_X86_REG_FS, 0x1B),
                     (UC_X86_REG_SS, 0x20)):
        uc.reg_write(reg, sel)
    uc.reg_write(UC_X86_REG_FPCW, 0x027F)
    sp = VE.STACK_BASE + VE.STACK_SIZE - 0x1000
    uc.mem_write(sp, struct.pack("<II", VE.SENTINEL, G))
    uc.reg_write(UC_X86_REG_ESP, sp)
    imports, events = [], []

    def unmapped(u, access, addr, size, value, _):
        if addr in img.imports:
            imports.append(img.imports[addr])
        return False

    def on_code(u, addr, size, _):
        name = watch[addr]
        if name == "read":
            events.append((name, struct.unpack("<I", bytes(u.mem_read(u.reg_read(UC_X86_REG_ESP) + FRAME_STRICT, 4)))[0]))
        else:
            events.append((name, None))

    uc.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED | UC_HOOK_MEM_FETCH_UNMAPPED, unmapped)
    for addr in watch or {}:
        uc.hook_add(UC_HOOK_CODE, on_code, None, addr, addr)
    err = None
    try:
        uc.emu_start(ENTRY, VE.SENTINEL, count=4_000_000_000)
    except UcError as e:
        err = str(e)
    if err is None and uc.reg_read(UC_X86_REG_EIP) != VE.SENTINEL:
        err = "instruction limit"
    if imports:
        err = f"import {imports[0]}"
    return uc.reg_read(UC_X86_REG_EAX), err, bytes(uc.mem_read(VL.DATA_VA, VL.DATA_SIZE)), events


def sticky_reads(events) -> list[int]:
    """Indexes of the strict-flag reads that returned 1 although no store happened since the previous read: a scan that
    ran strict only because an EARLIER landing stored the flag (the second landing failed the condition)."""
    out, stored = [], False
    for k, (name, val) in enumerate(e for e in events if e[0] in ("set", "read")):
        if name == "set":
            stored = True
        else:
            if val == 1 and not stored:
                out.append(k)
            stored = False
    return out


# --------------------------------------------------------------------------------------------------------- default tests
def _negctl_changed(portlib, base, mode) -> list[int]:
    if not GOLDEN:
        pytest.skip("no pinned digests")
    return [s for s in SEEDS if _digest(*_port(portlib, _state_image(portlib, base, s), True, mode)) != GOLDEN[s]]



@need_exe
@pytest.mark.parametrize("seed", SEEDS)
def test_the_switch_changes_the_search(portlib, base, seed):
    """On each pinned state the search ends differently with the flag recomputed per landing (OFF) than with the
    original's sticky flag (ON): the result or the image differs, so U-1012's missing evidence exists."""
    img = _state_image(portlib, base, seed)
    on, off = _port(portlib, img, True), _port(portlib, img, False)
    assert on != off, f"seed {seed}: the switch changes nothing"
    assert _diff_symbols(portlib, on[1], off[1]) or on[0] != off[0]
    # a state of this family is a real search: a positive hole distance and an aim in {-1, 0, 1}
    assert on[0] in (0, 1, 0xFFFFFFFF)


@need_exe
@pytest.mark.parametrize("seed", SEEDS)
def test_golden_digests_of_the_original(portlib, base, seed):
    """The port with the switch ON equals the original's pinned result; with it OFF it does not (the control)."""
    if not GOLDEN:
        pytest.skip("no pinned digests (regenerate with SG_EMU_ORACLE=update, see the module notes)")
    img = _state_image(portlib, base, seed)
    assert _digest(*_port(portlib, img, True)) == GOLDEN[seed]
    assert _digest(*_port(portlib, img, False)) != GOLDEN[seed], "the fixed behaviour equals the original here"


@need_exe
@pytest.mark.parametrize("mode", sorted(NEGCTL_RED))
def test_golden_digests_see_negctl(portlib, base, mode):
    """chooseShotTarget's own wrong bodies change at least one pinned result (RED against the original's digests)."""
    assert _negctl_changed(portlib, base, mode), f"mode {mode} ({NEGCTL_RED[mode]}) is blind on every pinned state"


@need_exe
@pytest.mark.parametrize("mode", sorted(NEGCTL_BLIND))
def test_negctl_blind_spots_are_known(portlib, base, mode):
    """A blind mode that starts reading RED means the states grew: move it to NEGCTL_RED."""
    assert not _negctl_changed(portlib, base, mode), f"mode {mode} ({NEGCTL_BLIND[mode]}) is no longer blind"


# ------------------------------------------------------------------------------------------------------ opt-in oracle
@emu_oracle
@need_exe
@pytest.mark.parametrize("seed", SEEDS)
def test_original_equals_port_with_the_flag_sticky(portlib, base, seed):
    img = _state_image(portlib, base, seed)
    eax, err, after, _ = run_original(img)
    assert err is None, err
    pe, pimg = _port(portlib, img, True)
    assert pe == eax, f"seed {seed}: port eax {pe}, original {eax}"
    assert pimg == after, f"seed {seed}: GameState differs at {_diff_symbols(portlib, pimg, after)}"


@emu_oracle
@need_exe
@pytest.mark.parametrize("seed", SEEDS)
def test_original_differs_from_the_fixed_behaviour(portlib, base, seed):
    """The control: with the switch OFF the port is RED against the original on every pinned state."""
    img = _state_image(portlib, base, seed)
    eax, err, after, _ = run_original(img)
    assert err is None, err
    off = _port(portlib, img, False)
    assert off != (eax, after), f"seed {seed}: the fixed behaviour equals the original"


@emu_oracle
@need_exe
@pytest.mark.parametrize("seed", SEEDS)
def test_original_scans_strict_after_a_failing_landing(portlib, base, seed):
    """The mechanism, in the original: the strict flag is stored (once per qualifying landing) at one instruction and
    read at another before every scan; some read returns 1 with no store at its own landing."""
    img = _state_image(portlib, base, seed)
    _, err, _, events = run_original(img, {STRICT_STORE: "set", STRICT_READ: "read"})
    assert err is None, err
    reads = [v for n, v in events if n == "read"]
    assert reads and any(n == "set" for n, _ in events), "no landing ever met the strict condition"
    ever = own = False
    for name, val in events:
        if name == "set":
            ever = own = True
        else:
            assert own or ever or val == 0, "a scan before any store must see the zero-filled frame word"
            own = False
    assert sticky_reads(events), f"seed {seed}: every strict scan had its own store ({len(reads)} reads)"


def test_sticky_witness_has_no_false_positive():
    """sticky_reads on synthetic event lists: a read of 1 right after its own store is not sticky, a later one is."""
    assert sticky_reads([("read", 0), ("set", None), ("read", 1)]) == []
    assert sticky_reads([("read", 0), ("set", None), ("read", 1), ("read", 1)]) == [3]
    assert sticky_reads([("read", 0), ("read", 0)]) == []


@pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") != "update", reason="regeneration only (SG_EMU_ORACLE=update)")
@need_exe
def test_update_the_golden_digests(portlib, base):
    """`SG_EMU_ORACLE=update pytest port/test/test_shots_strict.py -k update -s` prints GOLDEN from the original's results."""
    out = {}
    for seed in SEEDS:
        eax, err, after, _ = run_original(_state_image(portlib, base, seed))
        assert err is None, f"seed {seed}: {err}"
        out[seed] = _digest(eax, after)
    print("GOLDEN = {")
    for seed, d in out.items():
        print(f'    {seed}: "{d}",')
    print("}")
