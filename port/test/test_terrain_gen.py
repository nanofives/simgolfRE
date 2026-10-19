"""Phase 3.1a terrain_gen module: generateRandomCourse (port/src/sim/terrain_gen.cpp).

The corpus has no usable vectors for it. Default tests run the port on fixed images and check what the specification
(re/analysis/terrain/00470a60_applyTerrainEdit.md) fixes: the course record and relief writes, the clubhouse, the
border sweeps, the saved tile table, the return value, the uninitialised-slot refusal, the negative controls.
The OPT-IN oracle (`SG_EMU_ORACLE=1`, unicorn + the user's original/golf_clean.exe) runs the ORIGINAL function with its
original callees under Unicorn over random GameState images and compares the whole data image and eax with the port.
"""
from __future__ import annotations

import os
import random
import struct
import zlib

import pytest

from port.test import lib as L
from port.test.test_terrain import VL, _m_golfers, _rand_image, need_exe, new_state, objs

pytestmark = pytest.mark.port
emu_oracle = pytest.mark.skipif(os.environ.get("SG_EMU_ORACLE") != "1",
                                reason="opt-in: SG_EMU_ORACLE=1 runs the original under Unicorn (development oracle)")

FN = "generateRandomCourse"
ADDR = 0x00470A60
REC = 0x2E                     # CourseRecord size
F_LOC, F_ACRES, F_RELIEF, F_SETTING, F_TYPE, F_YEAR = 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x22
NEGCTL = [(1, "camera y = column - 3"), (2, "border sweep skips the far column"), (3, "corner levels 50 x 50 only")]


def _u32(img, off):
    return struct.unpack_from("<I", img, off)[0]


def _i32(img, off):
    return struct.unpack_from("<i", img, off)[0]


def _course(portlib, img: bytearray, n: int, loc: int, acres: int, relief: int, setting: int, ctype: int) -> bytearray:
    o = objs(portlib)
    base = o["g_courseTable"][0] + REC * n
    for f, v in ((F_LOC, loc), (F_ACRES, acres), (F_RELIEF, relief), (F_SETTING, setting), (F_TYPE, ctype)):
        img[base + f] = v & 0xFF
    return img


def _put(portlib, img: bytearray, name: str, data: bytes) -> bytearray:
    off, size = objs(portlib)[name]
    assert len(data) <= size, name
    img[off:off + len(data)] = data
    return img


def _case_image(portlib, rng: random.Random, n: int, loc: int, ctype: int | None = None) -> bytearray:
    """A random image (test_terrain's grids and records, golfers) with course record n set up for generation."""
    img = bytearray(_rand_image(portlib, rng))
    img = _m_golfers(portlib)(rng, img)
    _course(portlib, img, n, loc, rng.randrange(256), rng.randrange(4), rng.randrange(3),
            rng.randrange(4) if ctype is None else ctype)
    o = objs(portlib)
    # sparse tile flags: the generator keeps every flag bit but 0x1000 / 0x100 / 0x20 (low byte of the clubhouse), so
    # dense random flags (0x8080, 0x400) make every clubhouse spot fail evalPlacementArea and the original never returns
    _put(portlib, img, "g_tileFlags", b"".join(
        struct.pack("<H", 0 if rng.random() < 0.97 else rng.choice([0x20, 0x80, 0x100, 0x200, 0x400, 0x1000, 0x8000]))
        for _ in range(2500)))
    # small building levels: a type >= 6 footprint grows by its level, and test_terrain's 0..119 covers the whole map with
    # the kind-9 building of c0 == 4, after which no spot passes evalPlacementArea (the original loops forever)
    _put(portlib, img, "g_objectTypeLevel", struct.pack("<20i", *[rng.randrange(0, 4) for _ in range(20)]))
    _put(portlib, img, "g_difficulty", struct.pack("<i", rng.randrange(4)))
    fl =_u32(img, o["g_flags"][0])
    fl = (fl | 0x1000000) if rng.random() < 0.3 else (fl & ~0x1000000)
    _put(portlib, img, "g_flags", struct.pack("<I", fl))
    _put(portlib, img, "g_rng", struct.pack("<I", rng.getrandbits(32)))
    _put(portlib, img, "g_year", struct.pack("<h", rng.randrange(-3, 40)))
    _put(portlib, img, "g_heightRelief", struct.pack("<i", rng.randrange(-0x40, 0x80)))
    return img


def port_call(portlib, image: bytes, n: int):
    """(eax or the SgError code as a negative tuple, final image)"""
    with new_state(portlib) as st:
        st.load_image(bytes(image))
        try:
            eax = st.call(FN, [n]).eax
        except L.SgError as e:
            return ("error", e.code), None
        return eax, st.image()


# ------------------------------------------------------------------------------------------- default state tests
def _fixed_image(portlib, n=3, loc=3, acres=40, relief=1, setting=0, ctype=0, difficulty=1, flag=False):
    with new_state(portlib) as st:
        img = bytearray(st.image())
    o = objs(portlib)
    _put(portlib, img, "g_tileTypeAttrs", bytes(img[o["f_tab_tileTypeAttrInit_4c1a40"][0]:][:23 * 0x30]))
    recs = struct.pack("<hhhBBii", -1, 0, 0, 0, 0, 0, 0) * 256
    _put(portlib, img, "g_placed_objects", recs)
    _course(portlib, img, n, loc, acres, relief, setting, ctype)
    _put(portlib, img, "g_difficulty", struct.pack("<i", difficulty))
    _put(portlib, img, "g_flags", struct.pack("<I", 0x1000000 if flag else 0))
    _put(portlib, img, "g_rng", struct.pack("<I", 0x1234567))
    _put(portlib, img, "g_year", struct.pack("<h", 7))
    return img


@need_exe
@pytest.mark.parametrize("setting", [0, 1, 2])
def test_generate_fixed_image_invariants(portlib, setting):
    n, acres = 3, 40
    img = _fixed_image(portlib, n=n, acres=acres, setting=setting)
    eax, out = port_call(portlib, img, n)
    assert out is not None, eax
    o = objs(portlib)
    rec = o["g_courseTable"][0] + REC * n
    assert eax == _u32(out, o["g_flags"][0])                         # rebuildHeightfield's eax: the new g_flags
    assert _i32(out, o["g_courseIndex"][0]) == n
    assert out[o["g_courseType"][0]] == 0 and out[rec + F_YEAR] == 7
    assert _i32(out, o["g_heightRelief"][0]) == 0x20 + 0x20 // 2       # relief 1, n / 4 == 0: + half
    types = out[o["g_tileType"][0]:][:2500]
    saved = out[o["g_tileTypeSaved"][0]:][:2500]
    # the clubhouse: hole 19's tee slots, its placed object, the camera
    hole19 = o["g_holes"][0] + 19 * 0x208
    cx, cy = _i32(out, hole19 + 8), _i32(out, hole19 + 0xC)
    assert (cx, cy) == (_i32(out, hole19 + 0x10), _i32(out, hole19 + 0x14))
    assert 0xF <= cx <= 0x1F and 0xF <= cy <= 0x1F
    assert (_i32(out, o["gCameraTileX"][0]), _i32(out, o["gCameraTileY"][0])) == (cx + 6, cy - 4)
    po = o["g_placed_objects"][0]
    club = [i for i in range(256) if struct.unpack_from("<hhh", out, po + 16 * i) == (0xF, cx, cy)]
    assert club and out[po + 16 * club[0] + 7] & 0x40
    # the walker spawnWalker(-6) returned is posted on the clubhouse
    wk = o["g_walkers"][0]
    assert any(out[wk + 0x4C * i + 0x10] == cx and out[wk + 0x4C * i + 0x11] == cy for i in range(64))
    # border sweeps: clearTile writes 0x11 (setting 2) else 0x14
    clear = 0x11 if setting == 2 else 0x14
    count, c = 0, 0x19
    while True:
        count += 1
        c -= 1
        if not c * c * 4 > acres * 10:
            break
    if setting != 2:
        for a in range(count):
            for s in range(50):
                assert types[a * 50 + s] == clear and types[(49 - a) * 50 + s] == clear
                assert types[s * 50 + 49 - a] == clear
                if setting == 0:
                    assert types[s * 50 + a] == clear
    else:  # island: per row x, clamp(|x - 25|, 8, 50) + count - 12 + Random::range(3) cells from each edge
        for x in range(50):
            for a in range(max(abs(x - 25), 8) + count - 12):
                for cxy in (a * 50 + x, x * 50 + a, (49 - a) * 50 + x, x * 50 + 49 - a):
                    assert types[cxy] == clear, (x, a)
    # g_tileTypeSaved is the table before the sweeps: equal wherever no sweep wrote
    kept = [i for i in range(2500) if types[i] != clear]
    assert len(kept) > 300 and all(saved[i] == types[i] for i in kept)
    # deterministic
    eax2, out2 = port_call(portlib, img, n)
    assert (eax2, out2) == (eax, out)


@need_exe
@pytest.mark.parametrize("ctype,n,flag,ok", [(4, 3, False, False), (4, 9, True, False), (5, 13, True, False),
                                             (-1, 2, False, False), (4, 8, False, True), (5, 12, False, True)])
def test_uninitialised_base_type_is_refused(portlib, ctype, n, flag, ok):
    """Course type 4 or outside 0..4 leaves the base type in an unwritten stack slot unless n >= 8 with the flag clear:
    the port raises SG_E_NO_RESOURCE (MissingResource) there instead of inventing a value."""
    img = _fixed_image(portlib, n=n, ctype=ctype, flag=flag)
    eax, out = port_call(portlib, img, n)
    if ok:
        assert out is not None, eax
    else:
        assert eax == ("error", -11)


@need_exe
@pytest.mark.parametrize("mode,what", NEGCTL)
def test_negctl_changes_the_result(portlib, mode, what):
    img = _fixed_image(portlib, setting=1)
    _, want = port_call(portlib, img, 3)
    try:
        assert portlib.negctl(FN, mode) == 0
        _, got = port_call(portlib, img, 3)
    finally:
        portlib.negctl_reset()
    assert got is not None and got != want, what


@need_exe
def test_no_clubhouse_spot_is_no_return(portlib):
    """Flag 0x8000 on every tile: evalPlacementArea refuses every clubhouse spot (rows / columns 15..31), so the
    original retries forever; the port proves the candidate set empty and raises SG_E_NO_RETURN."""
    img = _fixed_image(portlib)
    _put(portlib, img, "g_tileFlags", struct.pack("<H", 0x8000) * 2500)
    eax, out = port_call(portlib, img, 3)
    assert out is None and eax == ("error", -10)


def test_registered(portlib):
    assert FN in portlib.functions()
    assert portlib.negctl(FN, 0) == 0


# ------------------------------------------------------------------------------------------- opt-in emulator oracle
_IMG = None


def emu_call(image: bytes, n: int, budget: int = 500_000_000):
    """The original generateRandomCourse (and its original callees) over `image` in Unicorn, no memory hooks.
    -> (eax or None on a fault / import / budget, final data image)"""
    global _IMG
    import vector_emu as VE
    from unicorn import UC_ARCH_X86, UC_MODE_32, Uc, UcError
    from unicorn import x86_const as X
    if _IMG is None:
        _IMG = VE.load_image()
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(VL.IMAGE_BASE, VE.IMAGE_END - VL.IMAGE_BASE)
    uc.mem_write(VL.IMAGE_BASE, _IMG.data)
    uc.mem_write(VL.DATA_VA, bytes(image))
    uc.mem_map(VE.STACK_BASE, VE.STACK_SIZE)
    uc.mem_map(VE.SENTINEL, 0x1000)
    uc.mem_map(VE.TEB, 0x1000)
    uc.mem_map(VE.GDT, 0x1000)
    uc.mem_write(VE.TEB, struct.pack("<IIII", 0xFFFFFFFF, VE.STACK_BASE + VE.STACK_SIZE, VE.STACK_BASE, 0) + b"\0" * 8
                 + struct.pack("<I", VE.TEB))
    uc.mem_write(VE.GDT + 8, VE._gdt_entry(0, 0xFFFFF, 0xFB, 0xC))
    uc.mem_write(VE.GDT + 16, VE._gdt_entry(0, 0xFFFFF, 0xF3, 0xC))
    uc.mem_write(VE.GDT + 24, VE._gdt_entry(VE.TEB, 0xFFF, 0xF3, 0x4))
    uc.mem_write(VE.GDT + 32, VE._gdt_entry(0, 0xFFFFF, 0x93, 0xC))
    uc.reg_write(X.UC_X86_REG_GDTR, (0, VE.GDT, 0xFFF, 0))
    for reg, sel in ((X.UC_X86_REG_DS, 0x13), (X.UC_X86_REG_ES, 0x13), (X.UC_X86_REG_GS, 0x13),
                     (X.UC_X86_REG_FS, 0x1B), (X.UC_X86_REG_SS, 0x20)):
        uc.reg_write(reg, sel)
    uc.reg_write(X.UC_X86_REG_FPCW, 0x027F)
    sp = VE.STACK_BASE + VE.STACK_SIZE - 0x1000
    uc.mem_write(sp, struct.pack("<II", VE.SENTINEL, n & 0xFFFFFFFF))
    uc.reg_write(X.UC_X86_REG_ESP, sp)
    err = None
    try:
        uc.emu_start(ADDR, VE.SENTINEL, count=budget)
    except UcError as e:
        err = str(e)
    if err is None and uc.reg_read(X.UC_X86_REG_EIP) != VE.SENTINEL:
        err = "instruction budget"
    return (None if err else uc.reg_read(X.UC_X86_REG_EAX)), bytes(uc.mem_read(VL.DATA_VA, VL.DATA_SIZE)), err


def compare(portlib, image: bytes, n: int):
    """None when the port equals the emulated original (or both faulted), else a description."""
    ee, emu, err = emu_call(image, n)
    pe, port = port_call(portlib, image, n)
    if ee is None and port is None:
        return None
    if ee is None or port is None:
        return f"emulated {ee} ({err}), port {pe}"
    diff = [i for i in range(VL.DATA_SIZE) if port[i] != emu[i]] if port != emu else []
    if diff or pe != ee:
        names = sorted(objs(portlib).items(), key=lambda kv: kv[1][0])

        def sym(off):
            for nm, (o, s) in names:
                if o <= off < o + s:
                    return f"{nm}+0x{off - o:x}"
            return f"+0x{off:x}"
        return f"eax port 0x{pe:x} emu 0x{ee:x}; {len(diff)} bytes differ, first {[sym(d) for d in diff[:6]]}"
    return None


def oracle_cases():
    """(n, location_id c0, course type or None = random 0..3); every c0 0..16 (16 = none of the switch) three times,
    plus the course-type-4 / 5 bodies that do initialise the base type (n >= 8, flag cleared by the mutator below)."""
    cases = [(random.Random(c * 31 + k).randrange(17), c, None) for c in range(17) for k in range(3)]
    cases += [(random.Random(k).randrange(17), random.Random(k + 99).randrange(17), None) for k in range(40)]
    cases += [(8 + k % 9, k % 16, 4 + (k & 1)) for k in range(6)]
    return cases


def _image_for(portlib, k, case):
    n, c0, ctype = case
    rng = random.Random(zlib.crc32(f"{FN}:{k}".encode()))
    img = _case_image(portlib, rng, n, c0, ctype)
    if ctype is not None:          # types 4 / 5 initialise the base type only with the flag clear and n >= 8 (12 for 5)
        o = objs(portlib)
        _put(portlib, img, "g_flags", struct.pack("<I", _u32(img, o["g_flags"][0]) & ~0x1000000))
        if ctype == 5 and n < 12:
            n = 12 + n % 5
            _course(portlib, img, n, c0, img[o["g_courseTable"][0] + REC * n + F_ACRES], 1, 1, ctype)
    return img, n


@emu_oracle
@need_exe
@pytest.mark.parametrize("k", range(len(oracle_cases())))
def test_emulated_original_equals_port(portlib, k):
    img, n = _image_for(portlib, k, oracle_cases()[k])
    d = compare(portlib, bytes(img), n)
    assert d is None, f"case {k} {oracle_cases()[k]}: {d}"


@emu_oracle
@need_exe
def test_emulated_original_spins_where_the_port_says_no_return(portlib):
    """Dense random tile flags (0x8080 / 0x400 everywhere): the port's NoReturn and the original exhausting a 300 M
    instruction budget in the clubhouse search agree."""
    img, n = _image_for(portlib, 0, oracle_cases()[0])
    rng = random.Random(5)
    _put(portlib, img, "g_tileFlags", b"".join(struct.pack("<H", rng.getrandbits(16) | 0x8080) for _ in range(2500)))
    ee, _, err = emu_call(bytes(img), n, budget=300_000_000)
    pe, port = port_call(portlib, bytes(img), n)
    assert ee is None and err == "instruction budget"
    assert port is None and pe == ("error", -10)


@emu_oracle
@need_exe
@pytest.mark.parametrize("mode,what", NEGCTL)
def test_emulator_oracle_sees_negctl(portlib, mode, what):
    """Every wrong body reads RED against the emulated original on some of the first cases."""
    seen = 0
    try:
        portlib.negctl(FN, mode)
        for k in range(6):
            img, n = _image_for(portlib, k, oracle_cases()[k])
            if compare(portlib, bytes(img), n) is not None:
                seen += 1
    finally:
        portlib.negctl_reset()
    assert seen > 0, f"mode {mode} ({what}) is blind to the oracle"
